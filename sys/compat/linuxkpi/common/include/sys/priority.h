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

#ifndef _SYS_PRIORITY_H_
#define _SYS_PRIORITY_H_

/* DragonFly priority compatibility for LinuxKPI (FreeBSD priority) */

#include <sys/types.h>

/*
 * FreeBSD-style process priority definitions for LinuxKPI.
 * DragonFly uses different priority ranges, so we provide compatibility defines.
 */

/* Kernel thread priorities - FreeBSD compatible */
#define PRI_MIN         -128    /* Minimum priority */
#define PRI_MAX         127     /* Maximum priority */

/* Real-time priorities */
#define PRI_REALTIME    -64
#define PRI_TIMESHARE   0
#define PRI_IDLE        64
#define PRI_FIFO        -16     /* FIFO scheduling */

/* Priority classes */
#define PRI_ITHD        -128    /* Interrupt thread priority */
#define PRI_SOFTIRQ     -64     /* Soft interrupt priority */
#define PRI_BIO         -32     /* Block I/O priority */
#define PRI_NET         -16     /* Network interrupt priority */
#define PRI_CAM         -8      /* CAM (SCSI) priority */
#define PRI_TTY         -4      /* TTY priority */

/* User priorities */
#define PRI_USER_MIN    0
#define PRI_USER_MAX    127

/* Idle priority */
#define PRI_IDLE_KTHREAD  127

/* Priority manipulation macros */
#define PRI_BASE(class, pri)    (((class) << 8) | (pri))
#define PRI_CLASS(pri)          ((pri) >> 8)
#define PRI_LEVEL(pri)          ((pri) & 0xff)

/* Nice value conversions */
#define PRIO_MIN        -20
#define PRIO_MAX        20

/* Nice to priority conversion */
#define NICE_TO_PRIO(nice)      ((nice) + 20)
#define PRIO_TO_NICE(prio)      ((prio) - 20)

/* Real-time priority range */
#define RTP_PRIO_MIN    0
#define RTP_PRIO_MAX    31
#define RTP_PRIO_BASE   16

/* Scheduling policy constants */
#define SCHED_FIFO      1
#define SCHED_OTHER     2
#define SCHED_RR        3
#define SCHED_IDLE      4
#define SCHED_BATCH     5

/* ULE scheduler specific - not used in DragonFly */
#define PRI_CORE_IDLE   128
#define PRI_CORE_ITHD   -128

/* Thread priority helpers - stubs */
static __inline int
priority_user(int nice)
{
    return NICE_TO_PRIO(nice);
}

static __inline int
priority_kernel(int pri)
{
    return pri;
}

static __inline int
priority_ithd(int pri)
{
    return PRI_ITHD + pri;
}

/* RTP lookup - stub */
static __inline int
rtprio_to_pri(int type, int prio)
{
    switch (type) {
    case SCHED_FIFO:
    case SCHED_RR:
        return PRI_REALTIME + prio;
    case SCHED_IDLE:
        return PRI_IDLE;
    default:
        return PRI_TIMESHARE + prio;
    }
}

static __inline void
pri_to_rtprio(int pri, int *type, int *prio)
{
    if (pri < PRI_TIMESHARE) {
        *type = SCHED_FIFO;
        *prio = pri - PRI_REALTIME;
    } else if (pri >= PRI_IDLE) {
        *type = SCHED_IDLE;
        *prio = 0;
    } else {
        *type = SCHED_OTHER;
        *prio = pri;
    }
}

/* Process priority structure - compatibility */
#ifndef __DragonFly__
/* DragonFly already defines struct rtprio in sys/rtprio.h */
struct rtprio {
    uint16_t type;
    uint16_t prio;
};
#endif

/* RTP types */
#define RTP_PRIO_NORMAL     1
#define RTP_PRIO_REALTIME   2
#define RTP_PRIO_IDLE       3
#define RTP_PRIO_FIFO       4

/* Priority comparison helpers */
#define PRI_LESS(a, b)      ((a) < (b))
#define PRI_LESSEQUAL(a, b) ((a) <= (b))
#define PRI_GREATER(a, b)   ((a) > (b))
#define PRI_GREATEREQUAL(a, b)  ((a) >= (b))

/* Priority adjustment - stub */
static __inline int
priority_adjust(int pri, int adj)
{
    int newpri = pri + adj;
    if (newpri > PRI_MAX)
        newpri = PRI_MAX;
    if (newpri < PRI_MIN)
        newpri = PRI_MIN;
    return newpri;
}

/* Idle priority check */
static __inline int
priority_is_idle(int pri)
{
    return pri >= PRI_IDLE;
}

/* Real-time priority check */
static __inline int
priority_is_realtime(int pri)
{
    return pri < PRI_TIMESHARE;
}

#endif /* _SYS_PRIORITY_H_ */
