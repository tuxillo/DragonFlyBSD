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
 * ARM GICv2 Register Definitions
 * Minimal implementation for QEMU virt machine
 */

#ifndef _MACHINE_GIC_H_
#define _MACHINE_GIC_H_

/*
 * QEMU virt machine GICv2 addresses (hard-coded)
 */
#define GIC_DIST_BASE		0x08000000UL
#define GIC_CPU_BASE		0x08010000UL

/*
 * Distributor registers (GICD)
 */
#define GICD_CTLR		0x000	/* Control register */
#define GICD_TYPER		0x004	/* Type register */
#define GICD_IIDR		0x008	/* Implementer ID */
#define GICD_IGROUPR(n)		(0x080 + (n) * 4)	/* Group */
#define GICD_ISENABLER(n)	(0x100 + (n) * 4)	/* Set-enable */
#define GICD_ICENABLER(n)	(0x180 + (n) * 4)	/* Clear-enable */
#define GICD_ISPENDR(n)		(0x200 + (n) * 4)	/* Set-pending */
#define GICD_ICPENDR(n)		(0x280 + (n) * 4)	/* Clear-pending */
#define GICD_ISACTIVER(n)	(0x300 + (n) * 4)	/* Set-active */
#define GICD_ICACTIVER(n)	(0x380 + (n) * 4)	/* Clear-active */
#define GICD_IPRIORITYR(n)	(0x400 + (n))		/* Priority (byte access) */
#define GICD_ITARGETSR(n)	(0x800 + (n))		/* Target CPU (byte access) */
#define GICD_ICFGR(n)		(0xC00 + (n) * 4)	/* Config */
#define GICD_SGIR		0xF00			/* Software generated IRQ */

/*
 * CPU interface registers (GICC)
 */
#define GICC_CTLR		0x000	/* Control register */
#define GICC_PMR		0x004	/* Priority mask */
#define GICC_BPR		0x008	/* Binary point */
#define GICC_IAR		0x00C	/* Interrupt acknowledge */
#define GICC_EOIR		0x010	/* End of interrupt */
#define GICC_RPR		0x014	/* Running priority */
#define GICC_HPPIR		0x018	/* Highest pending priority */
#define GICC_ABPR		0x01C	/* Aliased binary point */
#define GICC_IIDR		0x0FC	/* Implementer ID */

/*
 * Distributor control register bits
 */
#define GICD_CTLR_ENABLE	0x1

/*
 * CPU interface control register bits
 */
#define GICC_CTLR_ENABLE	0x1

/*
 * Special IRQ values
 */
#define GIC_SPURIOUS_IRQ	1023
#define GIC_FIRST_PPI		16
#define GIC_FIRST_SPI		32

/*
 * Timer IRQ numbers (PPI = IRQ + 16)
 *
 * ARM Generic Timer uses PPIs:
 *   Secure Physical Timer:     PPI 29 (IRQ 13)
 *   Non-secure Physical Timer: PPI 30 (IRQ 14)
 *   Virtual Timer:             PPI 27 (IRQ 11)
 *   Hypervisor Timer:          PPI 26 (IRQ 10)
 */
#define GIC_VIRT_TIMER_IRQ	27	/* Virtual timer PPI */
#define GIC_PHYS_TIMER_IRQ	30	/* Physical timer PPI */
#define GIC_SEC_TIMER_IRQ	29	/* Secure physical timer PPI */
#define GIC_HYP_TIMER_IRQ	26	/* Hypervisor timer PPI */

#ifdef _KERNEL

#include <sys/serialize.h>

/*
 * Interrupt handler function type (same as driver_intr_t)
 */
typedef void (*gic_intr_handler_t)(void *);

/*
 * GIC interrupt registration structure (opaque handle)
 */
struct gic_irq_entry;

void	gic_init(void);
void	gic_enable_irq(int irq);
void	gic_disable_irq(int irq);
int	gic_get_irq(void);
void	gic_eoi(int irq);
void	arm64_irq_handler(void);

/*
 * Interrupt registration interface (for nexus bus_setup_intr)
 */
struct gic_irq_entry *gic_register_irq(int irq, gic_intr_handler_t handler,
		    void *arg, lwkt_serialize_t serializer);
void	gic_unregister_irq(struct gic_irq_entry *entry);

#endif /* _KERNEL */

#endif /* !_MACHINE_GIC_H_ */
