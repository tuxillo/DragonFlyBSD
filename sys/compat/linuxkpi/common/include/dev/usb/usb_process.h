/*-
 * Copyright (c) 2026 The DragonFly Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _USB_PROCESS_H_
#define _USB_PROCESS_H_

/* DragonFly USB process compatibility for LinuxKPI */

#include <sys/types.h>
#include <sys/proc.h>
#include <sys/thread.h>

/* USB process structure */
struct usb_process {
    struct proc *up_proc;
    struct thread *up_thread;
    void (*up_func)(void *arg);
    void *up_arg;
    int up_flags;
    int up_state;
    char up_name[32];
};

#define USB_PROCESS_FLAG_RUNNING 0x01
#define USB_PROCESS_FLAG_DYING 0x02

#define USB_PROCESS_STATE_STOPPED 0
#define USB_PROCESS_STATE_RUNNING 1

/* USB process operations - stubs */
static __inline int
usb_proc_create(struct usb_process *proc, void (*func)(void *), void *arg,
                int priority, const char *name)
{
    proc->up_func = func;
    proc->up_arg = arg;
    proc->up_flags = 0;
    proc->up_state = USB_PROCESS_STATE_STOPPED;
    strlcpy(proc->up_name, name, sizeof(proc->up_name));
    return 0;
}

static __inline void
usb_proc_free(struct usb_process *proc)
{
    /* Stub */
}

static __inline int
usb_proc_start(struct usb_process *proc)
{
    proc->up_flags |= USB_PROCESS_FLAG_RUNNING;
    proc->up_state = USB_PROCESS_STATE_RUNNING;
    return 0;
}

static __inline void
usb_proc_stop(struct usb_process *proc)
{
    proc->up_flags &= ~USB_PROCESS_FLAG_RUNNING;
    proc->up_state = USB_PROCESS_STATE_STOPPED;
}

static __inline int
usb_proc_is_running(struct usb_process *proc)
{
    return proc->up_state == USB_PROCESS_STATE_RUNNING;
}

static __inline int
usb_proc_is_dying(struct usb_process *proc)
{
    return proc->up_flags & USB_PROCESS_FLAG_DYING;
}

static __inline void
usb_proc_wait(struct usb_process *proc)
{
    /* Stub */
}

static __inline void
usb_proc_drain(struct usb_process *proc)
{
    /* Stub */
}

static __inline void
usb_proc_msignal(struct usb_process *proc, int sig)
{
    /* Stub */
}

static __inline int
usb_proc_mwait(struct usb_process *proc, int sig)
{
    return 0;  /* Stub */
}

/* USB process signal - stub */
static __inline void
usb_proc_signal(struct usb_process *proc)
{
    /* Stub */
}

/* USB process initialization - stub */
static __inline void
usb_proc_init(struct usb_process *proc)
{
    memset(proc, 0, sizeof(*proc));
}

/* USB process reaper - stub */
static __inline void
usb_proc_reaper(void)
{
    /* Stub */
}

/* USB process wake - stub */
static __inline void
usb_proc_wakeup(struct usb_process *proc)
{
    /* Stub */
}

/* USB process sleep - stub */
static __inline void
usb_proc_sleep(struct usb_process *proc, void *ident, const char *wmesg)
{
    /* Stub */
}

#endif /* _USB_PROCESS_H_ */
