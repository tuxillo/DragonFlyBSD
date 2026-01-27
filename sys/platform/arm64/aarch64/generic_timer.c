/*
 * Copyright (c) 2026 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@backplane.com>
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

/*
 * ARM64 Generic Timer Driver
 *
 * Uses the virtual timer (CNTV) for compatibility with hypervisors.
 * Implements DragonFly's cputimer and cputimer_intr interfaces.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/systimer.h>

#include <machine/clock.h>
#include <machine/gic.h>

/* Forward declarations */
static sysclock_t arm64_cputimer_count(void);
static void arm64_timer_intr_reload(struct cputimer_intr *, sysclock_t);
static void arm64_timer_intr_enable(struct cputimer_intr *);
static void arm64_timer_intr_config(struct cputimer_intr *,
    const struct cputimer *);
static void arm64_timer_intr_initclock(struct cputimer_intr *, boolean_t);

/* Global - called from gic.c IRQ handler */
void arm64_timer_intr(void *);

/*
 * tsc_frequency - high-resolution counter frequency.
 *
 * On ARM64 this is the generic timer counter frequency (CNTFRQ_EL0).
 * Referenced by generic kernel code in kern_clock.c to populate kpmap->tsc_freq.
 */
tsc_uclock_t tsc_frequency;

/*
 * tsc_present - indicates high-resolution counter is available.
 *
 * On ARM64 this is always true since the generic timer is mandatory per ARMv8.
 * Referenced by kern_nrandom.c for entropy collection via rdtsc().
 */
int tsc_present = 1;

/*
 * cputimer - free-running monotonic counter
 *
 * This provides the sys_cputimer interface using the ARM64 virtual counter.
 */
static struct cputimer arm64_cputimer = {
	SLIST_ENTRY_INITIALIZER,
	"ARM64",
	CPUTIMER_PRI_VMM,		/* High priority */
	CPUTIMER_ARM64,
	arm64_cputimer_count,
	cputimer_default_fromhz,
	cputimer_default_fromus,
	cputimer_default_construct,
	cputimer_default_destruct,
	0,				/* sync_base */
	0,				/* base */
	0,				/* freq - set at init */
	0,				/* freq64_usec */
	0,				/* freq64_nsec */
};

/*
 * cputimer_intr - one-shot interrupt timer
 *
 * This provides the sys_cputimer_intr interface using the ARM64 virtual timer.
 */
static struct cputimer_intr arm64_cputimer_intr = {
	.freq		= 0,		/* Set at init */
	.reload		= arm64_timer_intr_reload,
	.enable		= arm64_timer_intr_enable,
	.config		= arm64_timer_intr_config,
	.restart	= cputimer_intr_default_restart,
	.pmfixup	= cputimer_intr_default_pmfixup,
	.initclock	= arm64_timer_intr_initclock,
	.pcpuhand	= NULL,
	.next		= SLIST_ENTRY_INITIALIZER,
	.name		= "ARM64",
	.type		= CPUTIMER_INTR_ARM64,
	.prio		= CPUTIMER_INTR_PRIO_VMM,
	.caps		= CPUTIMER_INTR_CAP_PS,	/* Works during power saving */
	.priv		= NULL,
};

/*
 * Return the current counter value.
 */
static sysclock_t
arm64_cputimer_count(void)
{
	return (sysclock_t)arm64_read_cntvct();
}

/*
 * Reload the timer with a new countdown value.
 *
 * The reload value is in sys_cputimer->freq ticks.
 */
static void
arm64_timer_intr_reload(struct cputimer_intr *cti __unused, sysclock_t reload)
{
	/*
	 * Mask the interrupt first to prevent spurious fires.
	 */
	arm64_write_cntv_ctl(GT_CTRL_INT_MASK);

	if (reload > 0) {
		/*
		 * Set countdown value and enable timer.
		 * TVAL is a signed 32-bit countdown register.
		 */
		arm64_write_cntv_tval((int32_t)reload);
		arm64_write_cntv_ctl(GT_CTRL_ENABLE);
	}
}

/*
 * Enable timer interrupts (called once per CPU during boot).
 */
static void
arm64_timer_intr_enable(struct cputimer_intr *cti __unused)
{
	/*
	 * Enable the virtual timer IRQ in the GIC.
	 */
	gic_enable_irq(GIC_VIRT_TIMER_IRQ);
}

/*
 * Configure the interrupt timer for a new cputimer.
 */
static void
arm64_timer_intr_config(struct cputimer_intr *cti __unused,
    const struct cputimer *timer __unused)
{
	/* Nothing to do - frequency is fixed */
}

/*
 * Initialize clock (called during SI_BOOT2_CLOCKREG).
 */
static void
arm64_timer_intr_initclock(struct cputimer_intr *cti __unused,
    boolean_t selected)
{
	if (!selected)
		return;

	/*
	 * Mask interrupt until first reload.
	 */
	arm64_write_cntv_ctl(GT_CTRL_INT_MASK);
}

/*
 * Timer interrupt handler.
 *
 * Called from the IRQ exception vector when the virtual timer fires.
 */
void
arm64_timer_intr(void *frame)
{
	/*
	 * Mask the interrupt to acknowledge.
	 * The timer will be re-armed by systimer code via reload().
	 */
	arm64_write_cntv_ctl(GT_CTRL_INT_MASK);

	/*
	 * Process system timers.
	 * This calls into kern_cputimer.c to dispatch systimers.
	 */
	systimer_intr(NULL, 0, frame);
}

/*
 * Initialize the timer subsystem.
 *
 * Called at SI_BOOT2_CLOCKREG to register our cputimer and cputimer_intr.
 */
static void
arm64_timer_init(void *dummy __unused)
{
	uint32_t freq;

	/*
	 * Get the timer frequency from the firmware-programmed register.
	 * QEMU typically sets this to ~62.5 MHz for the virt machine.
	 */
	freq = arm64_read_cntfrq();
	if (freq == 0) {
		kprintf("ARM64 timer: WARNING - CNTFRQ not set by firmware!\n");
		/* Use a reasonable default for QEMU virt */
		freq = 62500000;
	}

	kprintf("ARM64 timer: frequency %u Hz (%u.%u MHz)\n",
	    freq, freq / 1000000, (freq / 1000) % 1000);

	/*
	 * Register and select the interrupt timer FIRST.
	 *
	 * This must happen before cputimer_select() because that function
	 * calls systimer_changed() which calls cputimer_intr_reload(),
	 * and cputimer_intr_reload() requires sys_cputimer_intr to be set.
	 */
	arm64_cputimer_intr.freq = freq;
	kprintf("ARM64 timer: calling cputimer_intr_register\n");
	cputimer_intr_register(&arm64_cputimer_intr);
	kprintf("ARM64 timer: calling cputimer_intr_select\n");
	cputimer_intr_select(&arm64_cputimer_intr, 0);

	/*
	 * Now register and select the free-running cputimer.
	 */
	arm64_cputimer.freq = freq;

	/*
	 * Set tsc_frequency for generic kernel code (kern_clock.c).
	 * On ARM64 this is the generic timer counter frequency.
	 */
	tsc_frequency = freq;

	kprintf("ARM64 timer: calling cputimer_register\n");
	cputimer_register(&arm64_cputimer);
	kprintf("ARM64 timer: calling cputimer_select\n");
	cputimer_select(&arm64_cputimer, 0);

	kprintf("ARM64 timer: registered cputimer and cputimer_intr\n");
}

/*
 * Register as SI_BOOT2_CLOCKREG, SI_ORDER_FIRST.
 * This runs early in boot to set up timers before they're needed.
 */
SYSINIT(arm64_timer, SI_BOOT2_CLOCKREG, SI_ORDER_FIRST, arm64_timer_init, NULL);
