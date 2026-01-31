/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2014 Dmitry Chagin <dchagin@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/event.h>
#include <sys/eventfd.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/filio.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/mutex2.h>
#include <sys/proc.h>
#include <sys/refcount.h>
#include <sys/selinfo.h>
#include <sys/stat.h>
#include <sys/uio.h>

/*
 * Note: EFD_CLOEXEC and EFD_NONBLOCK values differ between FreeBSD and
 * DragonFly. FreeBSD matches them to O_CLOEXEC/O_NONBLOCK, but DragonFly
 * uses different values. The eventfd flags are internal to the eventfd
 * implementation and don't need to match the file flags.
 */

MALLOC_DEFINE(M_EVENTFD, "eventfd", "eventfd structures");

static int	eventfd_read(struct file *fp, struct uio *uio,
		    struct ucred *active_cred, int flags);
static int	eventfd_write(struct file *fp, struct uio *uio,
		    struct ucred *active_cred, int flags);
static int	eventfd_ioctl(struct file *fp, u_long cmd, caddr_t data,
		    struct ucred *active_cred, struct sysmsg *msg);
static int	eventfd_kqfilter(struct file *fp, struct knote *kn);
static int	eventfd_stat(struct file *fp, struct stat *st,
		    struct ucred *active_cred);
static int	eventfd_close(struct file *fp);

static const struct fileops eventfdops = {
	.fo_read = eventfd_read,
	.fo_write = eventfd_write,
	.fo_ioctl = eventfd_ioctl,
	.fo_kqfilter = eventfd_kqfilter,
	.fo_stat = eventfd_stat,
	.fo_close = eventfd_close,
	.fo_shutdown = nofo_shutdown,
	.fo_seek = badfo_seek,
	.fo_flags = DFLAG_PASSABLE,
};

static void	filt_eventfddetach(struct knote *kn);
static int	filt_eventfdread(struct knote *kn, long hint);
static int	filt_eventfdwrite(struct knote *kn, long hint);

/*
 * DragonFly filterops use { f_flags, f_attach, f_detach, f_event }
 * FILTEROP_ISFD indicates the ident is a file descriptor.
 */
static struct filterops eventfd_rfiltops =
	{ FILTEROP_ISFD, NULL, filt_eventfddetach, filt_eventfdread };

static struct filterops eventfd_wfiltops =
	{ FILTEROP_ISFD, NULL, filt_eventfddetach, filt_eventfdwrite };

struct eventfd {
	eventfd_t	efd_count;
	u_int32_t	efd_flags;
	struct selinfo	efd_sel;
	struct mtx	efd_lock;
	unsigned int	efd_refcount;
};

int
eventfd_create_file(struct thread *td, struct file *fp, u_int32_t initval,
    int flags)
{
	struct eventfd *efd;
	int fflags;

	(void)td;

	efd = kmalloc(sizeof(*efd), M_EVENTFD, M_WAITOK | M_ZERO);
	efd->efd_flags = flags;
	efd->efd_count = initval;
	mtx_init(&efd->efd_lock, "eventfd");
	knlist_init_mtx(&efd->efd_sel.si_note, &efd->efd_lock);
	refcount_init(&efd->efd_refcount, 1);

	fflags = FREAD | FWRITE;
	if ((flags & EFD_NONBLOCK) != 0)
		fflags |= FNONBLOCK;

	/* Initialize file structure directly (DragonFly doesn't have finit) */
	fp->f_flag = fflags;
	fp->f_type = DTYPE_EVENTFD;
	fp->f_data = efd;
	fp->f_ops = __DECONST(struct fileops *, &eventfdops);

	return (0);
}

struct eventfd *
eventfd_get(struct file *fp)
{
	struct eventfd *efd;

	if (fp->f_data == NULL || fp->f_ops != &eventfdops)
		return (NULL);

	efd = fp->f_data;
	refcount_acquire(&efd->efd_refcount);

	return (efd);
}

void
eventfd_put(struct eventfd *efd)
{
	if (!refcount_release(&efd->efd_refcount))
		return;

	seldrain(&efd->efd_sel);
	knlist_destroy(&efd->efd_sel.si_note);
	mtx_uninit(&efd->efd_lock);
	kfree(efd, M_EVENTFD);
}

static void
eventfd_wakeup(struct eventfd *efd)
{
	KNOTE_LOCKED(&efd->efd_sel.si_note, 0);
	selwakeup(&efd->efd_sel);
	wakeup(&efd->efd_count);
}

void
eventfd_signal(struct eventfd *efd)
{
	mtx_lock(&efd->efd_lock);

	if (efd->efd_count < UINT64_MAX)
		efd->efd_count++;

	eventfd_wakeup(efd);

	mtx_unlock(&efd->efd_lock);
}

static int
eventfd_close(struct file *fp)
{
	struct eventfd *efd;

	efd = fp->f_data;
	eventfd_put(efd);
	return (0);
}

static int
eventfd_read(struct file *fp, struct uio *uio, struct ucred *active_cred,
    int flags)
{
	struct eventfd *efd;
	eventfd_t count;
	int error;

	(void)active_cred;
	(void)flags;

	if (uio->uio_resid < sizeof(eventfd_t))
		return (EINVAL);

	error = 0;
	efd = fp->f_data;
	mtx_lock(&efd->efd_lock);
	while (error == 0 && efd->efd_count == 0) {
		if ((fp->f_flag & FNONBLOCK) != 0) {
			mtx_unlock(&efd->efd_lock);
			return (EAGAIN);
		}
		error = mtxsleep(&efd->efd_count, &efd->efd_lock, PCATCH,
		    "efdrd", 0);
	}
	if (error == 0) {
		if ((efd->efd_flags & EFD_SEMAPHORE) != 0) {
			count = 1;
			--efd->efd_count;
		} else {
			count = efd->efd_count;
			efd->efd_count = 0;
		}
		eventfd_wakeup(efd);
		mtx_unlock(&efd->efd_lock);
		error = uiomove((caddr_t)&count, sizeof(eventfd_t), uio);
	} else {
		mtx_unlock(&efd->efd_lock);
	}

	return (error);
}

static int
eventfd_write(struct file *fp, struct uio *uio, struct ucred *active_cred,
    int flags)
{
	struct eventfd *efd;
	eventfd_t count;
	int error;

	(void)active_cred;
	(void)flags;

	if (uio->uio_resid < sizeof(eventfd_t))
		return (EINVAL);

	error = uiomove((caddr_t)&count, sizeof(eventfd_t), uio);
	if (error != 0)
		return (error);
	if (count == UINT64_MAX)
		return (EINVAL);

	efd = fp->f_data;
	mtx_lock(&efd->efd_lock);
retry:
	if (UINT64_MAX - efd->efd_count <= count) {
		if ((fp->f_flag & FNONBLOCK) != 0) {
			mtx_unlock(&efd->efd_lock);
			uio->uio_resid += sizeof(eventfd_t);
			return (EAGAIN);
		}
		error = mtxsleep(&efd->efd_count, &efd->efd_lock,
		    PCATCH, "efdwr", 0);
		if (error == 0)
			goto retry;
	}
	if (error == 0) {
		efd->efd_count += count;
		eventfd_wakeup(efd);
	}
	mtx_unlock(&efd->efd_lock);

	return (error);
}

static int
eventfd_kqfilter(struct file *fp, struct knote *kn)
{
	struct eventfd *efd = fp->f_data;

	mtx_lock(&efd->efd_lock);
	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &eventfd_rfiltops;
		break;
	case EVFILT_WRITE:
		kn->kn_fop = &eventfd_wfiltops;
		break;
	default:
		mtx_unlock(&efd->efd_lock);
		return (EINVAL);
	}

	kn->kn_hook = (caddr_t)efd;
	knlist_add(&efd->efd_sel.si_note, kn, 1);
	mtx_unlock(&efd->efd_lock);

	return (0);
}

static void
filt_eventfddetach(struct knote *kn)
{
	struct eventfd *efd = (struct eventfd *)kn->kn_hook;

	mtx_lock(&efd->efd_lock);
	knlist_remove(&efd->efd_sel.si_note, kn, 1);
	mtx_unlock(&efd->efd_lock);
}

static int
filt_eventfdread(struct knote *kn, long hint)
{
	struct eventfd *efd = (struct eventfd *)kn->kn_hook;
	int ret;

	(void)hint;

	kn->kn_data = (int64_t)efd->efd_count;
	ret = efd->efd_count > 0;

	return (ret);
}

static int
filt_eventfdwrite(struct knote *kn, long hint)
{
	struct eventfd *efd = (struct eventfd *)kn->kn_hook;
	int ret;

	(void)hint;

	kn->kn_data = (int64_t)(UINT64_MAX - 1 - efd->efd_count);
	ret = UINT64_MAX - 1 > efd->efd_count;

	return (ret);
}

static int
eventfd_ioctl(struct file *fp, u_long cmd, caddr_t data,
    struct ucred *active_cred, struct sysmsg *msg)
{
	(void)fp;
	(void)data;
	(void)active_cred;
	(void)msg;

	switch (cmd) {
	case FIONBIO:
	case FIOASYNC:
		return (0);
	}

	return (ENOTTY);
}

static int
eventfd_stat(struct file *fp, struct stat *st, struct ucred *active_cred)
{
	(void)fp;
	(void)active_cred;

	bzero((void *)st, sizeof(*st));
	st->st_mode = S_IFIFO;
	return (0);
}
