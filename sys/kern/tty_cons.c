/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)cons.c	7.2 (Berkeley) 5/9/91
 * $FreeBSD: src/sys/kern/tty_cons.c,v 1.81.2.4 2001/12/17 18:44:41 guido Exp $
 * $DragonFly: src/sys/kern/tty_cons.c,v 1.9 2003/11/20 06:05:30 dillon Exp $
 */

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/sysctl.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <sys/msgport.h>
#include <sys/msgport2.h>
#include <sys/device.h>

#include <ddb/ddb.h>

#include <machine/cpu.h>

static	d_open_t	cnopen;
static	d_close_t	cnclose;
static	d_read_t	cnread;
static	d_write_t	cnwrite;
static	d_ioctl_t	cnioctl;
static	d_poll_t	cnpoll;
static	d_kqfilter_t	cnkqfilter;
static int console_putport(lwkt_port_t port, lwkt_msg_t lmsg);

#define	CDEV_MAJOR	0
static struct cdevsw cn_cdevsw = {
	/* name */	"console",
	/* maj */	CDEV_MAJOR,
	/* flags */	D_TTY | D_KQFILTER,
	/* port */	NULL,
	/* autoq */	0,

	/* open */	cnopen,
	/* close */	cnclose,
	/* read */	cnread,
	/* write */	cnwrite,
	/* ioctl */	cnioctl,
	/* poll */	cnpoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* kqfilter */	cnkqfilter
};

static dev_t	cn_dev_t; 	/* seems to be never really used */
static udev_t	cn_udev_t;
SYSCTL_OPAQUE(_machdep, CPU_CONSDEV, consdev, CTLFLAG_RD,
	&cn_udev_t, sizeof cn_udev_t, "T,dev_t", "");

static int cn_mute;

int	cons_unavail = 0;	/* XXX:
				 * physical console not available for
				 * input (i.e., it is in graphics mode)
				 */

static u_char cn_is_open;		/* nonzero if logical console is open */
static int openmode, openflag;		/* how /dev/console was openned */
static dev_t cn_devfsdev;		/* represents the device private info */
static u_char cn_phys_is_open;		/* nonzero if physical device is open */
       struct consdev *cn_tab;		/* physical console device info */
static u_char console_pausing;		/* pause after each line during probe */
static char *console_pausestr=
"<pause; press any key to proceed to next line or '.' to end pause mode>";

static lwkt_port_t	cn_fwd_port;
static struct lwkt_port	cn_port;


CONS_DRIVER(cons, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
SET_DECLARE(cons_set, struct consdev);

void
cninit()
{
	struct consdev *best_cp, *cp, **list;

	/*
	 * Our port intercept
	 */
	lwkt_init_port(&cn_port, NULL);
	cn_port.mp_putport = console_putport;

	/*
	 * Find the first console with the highest priority.
	 */
	best_cp = NULL;
	SET_FOREACH(list, cons_set) {
		cp = *list;
		if (cp->cn_probe == NULL)
			continue;
		(*cp->cn_probe)(cp);
		if (cp->cn_pri > CN_DEAD &&
		    (best_cp == NULL || cp->cn_pri > best_cp->cn_pri))
			best_cp = cp;
	}

	/*
	 * Check if we should mute the console (for security reasons perhaps)
	 * It can be changes dynamically using sysctl kern.consmute
	 * once we are up and going.
	 * 
	 */
        cn_mute = ((boothowto & (RB_MUTE
			|RB_SINGLE
			|RB_VERBOSE
			|RB_ASKNAME
			|RB_CONFIG)) == RB_MUTE);
	
	/*
	 * If no console, give up.
	 */
	if (best_cp == NULL) {
		if (cn_tab != NULL && cn_tab->cn_term != NULL)
			(*cn_tab->cn_term)(cn_tab);
		cn_tab = best_cp;
		return;
	}

	/*
	 * Initialize console, then attach to it.  This ordering allows
	 * debugging using the previous console, if any.
	 */
	(*best_cp->cn_init)(best_cp);
	if (cn_tab != NULL && cn_tab != best_cp) {
		/* Turn off the previous console.  */
		if (cn_tab->cn_term != NULL)
			(*cn_tab->cn_term)(cn_tab);
	}
	if (boothowto & RB_PAUSE)
		console_pausing = 1;
	cn_tab = best_cp;
}

void
cninit_finish()
{
	if ((cn_tab == NULL) || cn_mute)
		return;

	/*
	 * Hook the open and close functions.
	 */
	if (dev_dport(cn_tab->cn_dev))
		cn_fwd_port = cdevsw_dev_override(cn_tab->cn_dev, &cn_port);
	cn_dev_t = cn_tab->cn_dev;
	cn_udev_t = dev2udev(cn_dev_t);
	console_pausing = 0;
}

static void
cnuninit(void)
{
	if (cn_tab == NULL)
		return;

	/*
	 * Unhook the open and close functions.
	 */
	cdevsw_dev_override(cn_tab->cn_dev, NULL);
	cn_fwd_port = NULL;
	cn_dev_t = NODEV;
	cn_udev_t = NOUDEV;
}

/*
 * User has changed the state of the console muting.
 * This may require us to open or close the device in question.
 */
static int
sysctl_kern_consmute(SYSCTL_HANDLER_ARGS)
{
	int error;
	int ocn_mute;

	ocn_mute = cn_mute;
	error = sysctl_handle_int(oidp, &cn_mute, 0, req);
	if((error == 0) && (cn_tab != NULL) && (req->newptr != NULL)) {
		if(ocn_mute && !cn_mute) {
			/*
			 * going from muted to unmuted.. open the physical dev 
			 * if the console has been openned
			 */
			cninit_finish();
			if(cn_is_open)
				/* XXX curproc is not what we want really */
				error = cnopen(cn_dev_t, openflag,
					openmode, curthread);
			/* if it failed, back it out */
			if ( error != 0) cnuninit();
		} else if (!ocn_mute && cn_mute) {
			/*
			 * going from unmuted to muted.. close the physical dev 
			 * if it's only open via /dev/console
			 */
			if(cn_is_open)
				error = cnclose(cn_dev_t, openflag,
					openmode, curthread);
			if ( error == 0) cnuninit();
		}
		if (error != 0) {
			/* 
	 		 * back out the change if there was an error
			 */
			cn_mute = ocn_mute;
		}
	}
	return (error);
}

SYSCTL_PROC(_kern, OID_AUTO, consmute, CTLTYPE_INT|CTLFLAG_RW,
	0, sizeof cn_mute, sysctl_kern_consmute, "I", "");

static int
console_putport(lwkt_port_t port, lwkt_msg_t lmsg)
{
	cdevallmsg_t msg = (cdevallmsg_t)lmsg;
	int error;

	switch(msg->am_lmsg.ms_cmd) {
	case CDEV_CMD_OPEN:
		error = cnopen(
			    msg->am_open.msg.dev,
			    msg->am_open.oflags,
			    msg->am_open.devtype,
			    msg->am_open.td
			);
		break;
	case CDEV_CMD_CLOSE:
		error = cnclose(
			    msg->am_close.msg.dev,
			    msg->am_close.fflag,
			    msg->am_close.devtype,
			    msg->am_close.td
			);
		break;
	default:
		 error = lwkt_forwardmsg(cn_fwd_port, &msg->am_lmsg);
		 break;
	}
	return(error);
}

static int
cnopen(dev, flag, mode, td)
	dev_t dev;
	int flag, mode;
	struct thread *td;
{
	dev_t cndev, physdev;
	int retval = 0;

	if (cn_tab == NULL || cn_fwd_port == NULL)
		return (0);
	cndev = cn_tab->cn_dev;
	physdev = (major(dev) == major(cndev) ? dev : cndev);
	/*
	 * If mute is active, then non console opens don't get here
	 * so we don't need to check for that. They 
	 * bypass this and go straight to the device.
	 */
	if(!cn_mute)
		retval = dev_port_dopen(cn_fwd_port, physdev, flag, mode, td);
	if (retval == 0) {
		/* 
		 * check if we openned it via /dev/console or 
		 * via the physical entry (e.g. /dev/sio0).
		 */
		if (dev == cndev)
			cn_phys_is_open = 1;
		else if (physdev == cndev) {
			openmode = mode;
			openflag = flag;
			cn_is_open = 1;
		}
		dev->si_tty = physdev->si_tty;
	}
	return (retval);
}

static int
cnclose(dev, flag, mode, td)
	dev_t dev;
	int flag, mode;
	struct thread *td;
{
	dev_t cndev;
	struct tty *cn_tp;

	if (cn_tab == NULL || cn_fwd_port == NULL)
		return (0);
	cndev = cn_tab->cn_dev;
	cn_tp = cndev->si_tty;
	/*
	 * act appropriatly depending on whether it's /dev/console
	 * or the pysical device (e.g. /dev/sio) that's being closed.
	 * in either case, don't actually close the device unless
	 * both are closed.
	 */
	if (dev == cndev) {
		/* the physical device is about to be closed */
		cn_phys_is_open = 0;
		if (cn_is_open) {
			if (cn_tp) {
				/* perform a ttyhalfclose() */
				/* reset session and proc group */
				cn_tp->t_pgrp = NULL;
				cn_tp->t_session = NULL;
			}
			return (0);
		}
	} else if (major(dev) != major(cndev)) {
		/* the logical console is about to be closed */
		cn_is_open = 0;
		if (cn_phys_is_open)
			return (0);
		dev = cndev;
	}
	if (cn_fwd_port)
		return (dev_port_dclose(cn_fwd_port, dev, flag, mode, td));
	return (0);
}

static int
cnread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{

	if (cn_tab == NULL || cn_fwd_port == NULL)
		return (0);
	dev = cn_tab->cn_dev;
	return (dev_port_dread(cn_fwd_port, dev, uio, flag));
}

static int
cnwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{

	if (cn_tab == NULL || cn_fwd_port == NULL) {
		uio->uio_resid = 0; /* dump the data */
		return (0);
	}
	if (constty)
		dev = constty->t_dev;
	else
		dev = cn_tab->cn_dev;
	log_console(uio);
	return (dev_port_dwrite(cn_fwd_port, dev, uio, flag));
}

static int
cnioctl(dev, cmd, data, flag, td)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct thread *td;
{
	int error;

	if (cn_tab == NULL || cn_fwd_port == NULL)
		return (0);
	KKASSERT(td->td_proc != NULL);
	/*
	 * Superuser can always use this to wrest control of console
	 * output from the "virtual" console.
	 */
	if (cmd == TIOCCONS && constty) {
		error = suser(td);
		if (error)
			return (error);
		constty = NULL;
		return (0);
	}
	dev = cn_tab->cn_dev;
	return (dev_port_dioctl(cn_fwd_port, dev, cmd, data, flag, td));
}

static int
cnpoll(dev, events, td)
	dev_t dev;
	int events;
	struct thread *td;
{
	if ((cn_tab == NULL) || cn_mute || cn_fwd_port == NULL)
		return (1);

	dev = cn_tab->cn_dev;

	return (dev_port_dpoll(cn_fwd_port, dev, events, td));
}

static int
cnkqfilter(dev, kn)
	dev_t dev;
	struct knote *kn;
{
	if ((cn_tab == NULL) || cn_mute || cn_fwd_port == NULL)
		return (1);

	dev = cn_tab->cn_dev;
	return (dev_port_dkqfilter(cn_fwd_port, dev, kn));
}

int
cngetc()
{
	int c;
	if ((cn_tab == NULL) || cn_mute)
		return (-1);
	c = (*cn_tab->cn_getc)(cn_tab->cn_dev);
	if (c == '\r') c = '\n'; /* console input is always ICRNL */
	return (c);
}

int
cncheckc()
{
	if ((cn_tab == NULL) || cn_mute)
		return (-1);
	return ((*cn_tab->cn_checkc)(cn_tab->cn_dev));
}

void
cnputc(c)
	int c;
{
	char *cp;

	if ((cn_tab == NULL) || cn_mute)
		return;
	if (c) {
		if (c == '\n')
			(*cn_tab->cn_putc)(cn_tab->cn_dev, '\r');
		(*cn_tab->cn_putc)(cn_tab->cn_dev, c);
#ifdef DDB
		if (console_pausing && !db_active && (c == '\n')) {
#else
		if (console_pausing && (c == '\n')) {
#endif
			for(cp=console_pausestr; *cp != '\0'; cp++)
			    (*cn_tab->cn_putc)(cn_tab->cn_dev, *cp);
			if (cngetc() == '.')
				console_pausing = 0;
			(*cn_tab->cn_putc)(cn_tab->cn_dev, '\r');
			for(cp=console_pausestr; *cp != '\0'; cp++)
			    (*cn_tab->cn_putc)(cn_tab->cn_dev, ' ');
			(*cn_tab->cn_putc)(cn_tab->cn_dev, '\r');
		}
	}
}

void
cndbctl(on)
	int on;
{
	static int refcount;

	if (cn_tab == NULL)
		return;
	if (!on)
		refcount--;
	if (refcount == 0 && cn_tab->cn_dbctl != NULL)
		(*cn_tab->cn_dbctl)(cn_tab->cn_dev, on);
	if (on)
		refcount++;
}

static void
cn_drvinit(void *unused)
{

	cn_devfsdev = make_dev(&cn_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600,
	    "console");
}

SYSINIT(cndev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,cn_drvinit,NULL)
