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
 * Minimal ARM GICv2 driver for QEMU virt machine.
 *
 * This is a bare-minimum driver that only handles timer interrupts.
 * It uses hard-coded addresses for QEMU virt and does not do dynamic
 * discovery via FDT or ACPI.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <machine/gic.h>
#include <machine/vmparam.h>

/* Pointers to GIC registers via DMAP */
static volatile uint32_t *gic_dist;
static volatile uint32_t *gic_cpu;

/* Track initialization state */
static int gic_initialized;

/*
 * Register access functions
 */
static __inline uint32_t
gic_dist_read(uint32_t reg)
{
	return gic_dist[reg / 4];
}

static __inline void
gic_dist_write(uint32_t reg, uint32_t val)
{
	gic_dist[reg / 4] = val;
}

static __inline uint32_t
gic_cpu_read(uint32_t reg)
{
	return gic_cpu[reg / 4];
}

static __inline void
gic_cpu_write(uint32_t reg, uint32_t val)
{
	gic_cpu[reg / 4] = val;
}

/*
 * Initialize the GIC.
 *
 * Must be called after DMAP is set up but before timer registration.
 */
void
gic_init(void)
{
	int i;

	if (gic_initialized)
		return;

	/*
	 * Map GIC registers via DMAP.
	 * GIC is at fixed addresses on QEMU virt machine.
	 */
	gic_dist = (volatile uint32_t *)PHYS_TO_DMAP(GIC_DIST_BASE);
	gic_cpu = (volatile uint32_t *)PHYS_TO_DMAP(GIC_CPU_BASE);

	kprintf("GIC: mapping dist=%p cpu=%p\n",
	    (void *)gic_dist, (void *)gic_cpu);

	/*
	 * Disable distributor while configuring.
	 */
	gic_dist_write(GICD_CTLR, 0);

	/*
	 * Set all PPI/SGI priorities to middle (128).
	 * PPIs are IRQs 16-31, SGIs are 0-15 (first 32 IRQs).
	 */
	for (i = 0; i < 32; i += 4) {
		gic_dist_write(GICD_IPRIORITYR(i), 0x80808080);
	}

	/*
	 * Disable all PPIs/SGIs initially.
	 */
	gic_dist_write(GICD_ICENABLER(0), 0xffffffff);

	/*
	 * Enable distributor.
	 */
	gic_dist_write(GICD_CTLR, GICD_CTLR_ENABLE);

	/*
	 * CPU interface setup:
	 * - Set priority mask to allow all priorities (0xFF)
	 * - Enable CPU interface
	 */
	gic_cpu_write(GICC_PMR, 0xFF);
	gic_cpu_write(GICC_CTLR, GICC_CTLR_ENABLE);

	gic_initialized = 1;
	kprintf("GIC: initialized\n");
}

/*
 * Enable a specific IRQ.
 */
void
gic_enable_irq(int irq)
{
	uint32_t reg = irq / 32;
	uint32_t bit = 1U << (irq % 32);

	gic_dist_write(GICD_ISENABLER(reg), bit);
}

/*
 * Disable a specific IRQ.
 */
void
gic_disable_irq(int irq)
{
	uint32_t reg = irq / 32;
	uint32_t bit = 1U << (irq % 32);

	gic_dist_write(GICD_ICENABLER(reg), bit);
}

/*
 * Acknowledge and get the current interrupt number.
 * Returns GIC_SPURIOUS_IRQ (1023) if no interrupt pending.
 */
int
gic_get_irq(void)
{
	uint32_t iar;

	iar = gic_cpu_read(GICC_IAR);
	return iar & 0x3FF;	/* IRQ number is bottom 10 bits */
}

/*
 * Signal End of Interrupt.
 */
void
gic_eoi(int irq)
{
	gic_cpu_write(GICC_EOIR, irq);
}

/*
 * IRQ dispatch handler - called from assembly exception_irq.
 *
 * This function:
 * 1. Acknowledges the IRQ via GICC_IAR
 * 2. Dispatches to the appropriate handler
 * 3. Signals EOI via GICC_EOIR
 */
void
arm64_irq_handler(void)
{
	int irq;

	/*
	 * Acknowledge interrupt and get IRQ number.
	 * Reading GICC_IAR also acknowledges the interrupt.
	 */
	irq = gic_get_irq();

	/* Check for spurious interrupt */
	if (irq == GIC_SPURIOUS_IRQ)
		return;

	/*
	 * Dispatch based on IRQ number.
	 * Currently we only handle the virtual timer.
	 */
	if (irq == GIC_VIRT_TIMER_IRQ) {
		/* Call the timer interrupt handler */
		extern void arm64_timer_intr(void *);
		arm64_timer_intr(NULL);
	} else {
		/* Unknown IRQ - just print a message for debugging */
		kprintf("GIC: unexpected IRQ %d\n", irq);
	}

	/* Signal End of Interrupt */
	gic_eoi(irq);
}
