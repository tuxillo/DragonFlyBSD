/*
 * Kernel interface to machine-dependent clock driver.
 * ARM64 Generic Timer register access.
 *
 * Copyright (c) 2026 The DragonFly Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _MACHINE_CLOCK_H_
#define	_MACHINE_CLOCK_H_

#ifndef _SYS_TYPES_H_
#include <sys/types.h>
#endif

/*
 * tsc_uclock_t - high-resolution timestamp type.
 *
 * On x86 this comes from the TSC (Time Stamp Counter).
 * On ARM64 we use CNTVCT_EL0 (virtual counter) which serves
 * the same purpose - a monotonically increasing counter.
 */
typedef uint64_t tsc_uclock_t;

#ifdef _KERNEL

#ifndef _SYS_SYSTIMER_H_
#include <sys/systimer.h>
#endif

typedef struct TOTALDELAY {
	int		us;
	int		started;
	sysclock_t	last_clock;
} TOTALDELAY;

/*
 * ARM64 Generic Timer control register bits
 */
#define GT_CTRL_ENABLE		(1 << 0)	/* Timer enable */
#define GT_CTRL_INT_MASK	(1 << 1)	/* Interrupt mask */
#define GT_CTRL_INT_STAT	(1 << 2)	/* Interrupt status */

/*
 * ARM64 Generic Timer register access functions
 *
 * We use the virtual timer (CNTV) for compatibility with hypervisors.
 * The virtual counter (CNTVCT) provides a free-running monotonic counter.
 */

/*
 * Read the virtual counter value.
 * This is the primary timekeeping source.
 */
static __inline uint64_t
arm64_read_cntvct(void)
{
	uint64_t val;
	__asm __volatile("isb; mrs %0, cntvct_el0" : "=r"(val));
	return val;
}

/*
 * Read the counter frequency (Hz).
 * This is set by firmware (QEMU sets it to 62.5 MHz typically).
 */
static __inline uint32_t
arm64_read_cntfrq(void)
{
	uint64_t val;
	__asm __volatile("mrs %0, cntfrq_el0" : "=r"(val));
	return (uint32_t)val;
}

/*
 * Write the virtual timer countdown value.
 * The timer fires when the countdown reaches zero.
 */
static __inline void
arm64_write_cntv_tval(int32_t val)
{
	__asm __volatile("msr cntv_tval_el0, %0; isb" :: "r"((uint64_t)val));
}

/*
 * Write the virtual timer control register.
 */
static __inline void
arm64_write_cntv_ctl(uint32_t val)
{
	__asm __volatile("msr cntv_ctl_el0, %0; isb" :: "r"((uint64_t)val));
}

/*
 * Read the virtual timer control register.
 */
static __inline uint32_t
arm64_read_cntv_ctl(void)
{
	uint64_t val;
	__asm __volatile("mrs %0, cntv_ctl_el0" : "=r"(val));
	return (uint32_t)val;
}

/*
 * Read the virtual timer countdown value.
 */
static __inline int32_t
arm64_read_cntv_tval(void)
{
	uint64_t val;
	__asm __volatile("mrs %0, cntv_tval_el0" : "=r"(val));
	return (int32_t)val;
}

/*
 * Read the physical counter value (for debugging).
 */
static __inline uint64_t
arm64_read_cntpct(void)
{
	uint64_t val;
	__asm __volatile("isb; mrs %0, cntpct_el0" : "=r"(val));
	return val;
}

/*
 * Timecounter initialization structure for ARM64.
 */
typedef struct timecounter_init {
	char *name;
	void (*configure)(void);
} timecounter_init_t;

#define TIMECOUNTER_INIT(name, config)			\
	static struct timecounter_init name##_timer = {	\
		#name, config				\
	};					\
	DATA_SET(timecounter_init_set, name##_timer);

/*
 * tsc_frequency - high-resolution counter frequency.
 *
 * On x86 this is the TSC frequency. On ARM64 we use the
 * generic timer counter frequency (CNTFRQ_EL0).
 *
 * This variable is referenced by generic kernel code in kern_clock.c
 * to populate kpmap->tsc_freq.
 */
extern tsc_uclock_t tsc_frequency;

#endif /* _KERNEL */

#endif /* !_MACHINE_CLOCK_H_ */
