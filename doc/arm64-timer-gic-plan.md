# ARM64 Generic Timer and GIC Implementation Plan

## Overview

This document describes the implementation of ARM64 Generic Timer and
minimal GIC (Generic Interrupt Controller) support for the DragonFly kernel.

## Status: COMPLETE ✅

The timer and GIC implementation is complete and working. The kernel now:
- Initializes GIC distributor and CPU interface
- Registers the ARM64 Generic Timer as both cputimer and cputimer_intr
- Timer frequency detected correctly (24 MHz on QEMU virt)
- Timer interrupts fire correctly (scheduler runs)
- **Kernel boots successfully to `mountroot>` prompt**

## Problem Statement (Resolved)

The kernel was crashing at SI_BOOT2_POST_SMP (0x01cc0000) with an instruction abort:

```
!!! EXC ESR=000000008a000000 FAR=ffffffffffffffff ELR=ffffffffffffffff
EC=22 (IABT)
```

**Root cause:** `cputimer_intr_reload()` dereferences `sys_cputimer_intr`,
which was NULL because no timer driver had registered.

```c
// sys/kern/kern_cputimer.c:407-409
void cputimer_intr_reload(sysclock_t reload)
{
    struct cputimer_intr *cti = sys_cputimer_intr;
    cti->reload(cti, reload);  // NULL pointer dereference!
}
```

## Key Fix: Timer Registration Order

**Commit:** `eeb2db1d22` - arm64/timer: Fix initialization order

The original code registered the cputimer (counter) before the cputimer_intr
(interrupt timer). This caused a crash because `cputimer_select()` calls
`systimer_changed()` which calls `cputimer_intr_reload()`, but at that point
`sys_cputimer_intr` was still NULL.

**Fixed order (matching x86_64 i8254):**
1. `cputimer_intr_register()` - register interrupt timer
2. `cputimer_intr_select()` - select it (sets sys_cputimer_intr)
3. `cputimer_register()` - register counter
4. `cputimer_select()` - select it (calls systimer_changed, which now works)

## Solution Components

### 1. Minimal GIC Driver

A bare-minimum GICv2 driver to handle timer interrupts on QEMU virt.

### 2. Generic Timer Driver

An ARM64 Generic Timer driver using the virtual timer (CNTV).

## Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Timer type | Virtual (CNTV) | Works under hypervisors, FreeBSD default |
| GIC version | GICv2 | QEMU virt default, simpler than GICv3 |
| Discovery | Hard-coded | No FDT/ACPI infrastructure needed |
| Interrupt model | PPI only | Timer uses PPI 27, no SPI needed yet |

## QEMU virt Hardware Layout

```
GICv2 Distributor:  0x08000000 - 0x0800ffff (64KB)
GICv2 CPU Interface: 0x08010000 - 0x08011fff (8KB)

Timer IRQs (PPI = IRQ + 16):
  Virtual Timer:    PPI 27 (IRQ 11)
  Physical Timer:   PPI 30 (IRQ 14)
  Secure Physical:  PPI 29 (IRQ 13)
  Hypervisor:       PPI 26 (IRQ 10)
```

## File Structure

```
sys/platform/arm64/
├── aarch64/
│   ├── gic.c              # NEW: Minimal GIC driver
│   ├── generic_timer.c    # NEW: Timer driver
│   ├── machdep.c          # MODIFY: Init calls
│   └── locore.s           # MODIFY: IRQ exception vector
├── include/
│   ├── gic.h              # NEW: GIC definitions
│   └── clock.h            # NEW: Timer definitions
└── conf/
    └── files              # MODIFY: Add new sources
```

## Implementation Details

### GIC Register Definitions (gic.h)

```c
/*
 * ARM GICv2 Register Definitions
 * QEMU virt hard-coded addresses
 */

#define GIC_DIST_BASE       0x08000000
#define GIC_CPU_BASE        0x08010000

/* Distributor registers */
#define GICD_CTLR           0x000   /* Control register */
#define GICD_TYPER          0x004   /* Type register */
#define GICD_ISENABLER(n)   (0x100 + (n) * 4)  /* Set-enable */
#define GICD_ICENABLER(n)   (0x180 + (n) * 4)  /* Clear-enable */
#define GICD_ISPENDR(n)     (0x200 + (n) * 4)  /* Set-pending */
#define GICD_ICPENDR(n)     (0x280 + (n) * 4)  /* Clear-pending */
#define GICD_IPRIORITYR(n)  (0x400 + (n) * 4)  /* Priority */
#define GICD_ITARGETSR(n)   (0x800 + (n) * 4)  /* Target CPU */
#define GICD_ICFGR(n)       (0xC00 + (n) * 4)  /* Config */

/* CPU interface registers */
#define GICC_CTLR           0x000   /* Control register */
#define GICC_PMR            0x004   /* Priority mask */
#define GICC_BPR            0x008   /* Binary point */
#define GICC_IAR            0x00C   /* Interrupt acknowledge */
#define GICC_EOIR           0x010   /* End of interrupt */

/* Control bits */
#define GICD_CTLR_ENABLE    0x1
#define GICC_CTLR_ENABLE    0x1

/* Special IRQ values */
#define GIC_SPURIOUS_IRQ    1023

/* Timer IRQ numbers */
#define GIC_VIRT_TIMER_IRQ  27      /* PPI 11 + 16 */
```

### Minimal GIC Implementation (gic.c)

```c
/*
 * Minimal ARM GICv2 driver for QEMU virt
 * Only supports timer PPI - no dynamic IRQ management
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <machine/gic.h>
#include <machine/pmap.h>

static volatile uint32_t *gic_dist;
static volatile uint32_t *gic_cpu;

static inline uint32_t
gic_dist_read(uint32_t reg)
{
    return gic_dist[reg / 4];
}

static inline void
gic_dist_write(uint32_t reg, uint32_t val)
{
    gic_dist[reg / 4] = val;
}

static inline uint32_t
gic_cpu_read(uint32_t reg)
{
    return gic_cpu[reg / 4];
}

static inline void
gic_cpu_write(uint32_t reg, uint32_t val)
{
    gic_cpu[reg / 4] = val;
}

void
gic_init(void)
{
    /* Map GIC registers via DMAP */
    gic_dist = (volatile uint32_t *)PHYS_TO_DMAP(GIC_DIST_BASE);
    gic_cpu = (volatile uint32_t *)PHYS_TO_DMAP(GIC_CPU_BASE);
    
    /* Disable distributor */
    gic_dist_write(GICD_CTLR, 0);
    
    /* Set all IRQ priorities to middle (128) */
    for (int i = 0; i < 32; i += 4) {
        gic_dist_write(GICD_IPRIORITYR(i), 0x80808080);
    }
    
    /* Enable distributor */
    gic_dist_write(GICD_CTLR, GICD_CTLR_ENABLE);
    
    /* Enable CPU interface, allow all priorities */
    gic_cpu_write(GICC_PMR, 0xFF);
    gic_cpu_write(GICC_CTLR, GICC_CTLR_ENABLE);
    
    kprintf("GIC: initialized (dist=%p cpu=%p)\n", gic_dist, gic_cpu);
}

void
gic_enable_irq(int irq)
{
    uint32_t reg = irq / 32;
    uint32_t bit = 1 << (irq % 32);
    
    gic_dist_write(GICD_ISENABLER(reg), bit);
}

void
gic_disable_irq(int irq)
{
    uint32_t reg = irq / 32;
    uint32_t bit = 1 << (irq % 32);
    
    gic_dist_write(GICD_ICENABLER(reg), bit);
}

int
gic_get_irq(void)
{
    uint32_t iar = gic_cpu_read(GICC_IAR);
    return iar & 0x3FF;  /* IRQ number is bottom 10 bits */
}

void
gic_eoi(int irq)
{
    gic_cpu_write(GICC_EOIR, irq);
}
```

### Timer Register Access (clock.h)

```c
/*
 * ARM64 Generic Timer Register Access
 */

#ifndef _MACHINE_CLOCK_H_
#define _MACHINE_CLOCK_H_

/* Timer control bits */
#define GT_CTRL_ENABLE      (1 << 0)
#define GT_CTRL_INT_MASK    (1 << 1)
#define GT_CTRL_INT_STAT    (1 << 2)

static inline uint64_t
arm64_read_cntvct(void)
{
    uint64_t val;
    __asm __volatile("isb; mrs %0, cntvct_el0" : "=r"(val));
    return val;
}

static inline uint32_t
arm64_read_cntfrq(void)
{
    uint64_t val;
    __asm __volatile("mrs %0, cntfrq_el0" : "=r"(val));
    return (uint32_t)val;
}

static inline void
arm64_write_cntv_tval(int32_t val)
{
    __asm __volatile("msr cntv_tval_el0, %0; isb" :: "r"((uint64_t)val));
}

static inline void
arm64_write_cntv_ctl(uint32_t val)
{
    __asm __volatile("msr cntv_ctl_el0, %0; isb" :: "r"((uint64_t)val));
}

static inline uint32_t
arm64_read_cntv_ctl(void)
{
    uint64_t val;
    __asm __volatile("mrs %0, cntv_ctl_el0" : "=r"(val));
    return (uint32_t)val;
}

#endif /* _MACHINE_CLOCK_H_ */
```

### Timer Implementation (generic_timer.c)

```c
/*
 * ARM64 Generic Timer Driver
 * Uses virtual timer (CNTV) for compatibility with hypervisors
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
static void arm64_timer_intr_initclock(struct cputimer_intr *, boolean_t);

/* cputimer - free-running counter */
static struct cputimer arm64_cputimer = {
    .next       = SLIST_ENTRY_INITIALIZER,
    .name       = "ARM64",
    .pri        = CPUTIMER_PRI_VMM,
    .type       = CPUTIMER_ARM64,
    .count      = arm64_cputimer_count,
    .fromhz     = cputimer_default_fromhz,
    .fromus     = cputimer_default_fromus,
    .construct  = cputimer_default_construct,
    .destruct   = cputimer_default_destruct,
    .freq       = 0,
};

/* cputimer_intr - one-shot interrupt timer */
static struct cputimer_intr arm64_cputimer_intr = {
    .freq       = 0,
    .reload     = arm64_timer_intr_reload,
    .enable     = arm64_timer_intr_enable,
    .config     = cputimer_intr_default_config,
    .restart    = cputimer_intr_default_restart,
    .pmfixup    = cputimer_intr_default_pmfixup,
    .initclock  = arm64_timer_intr_initclock,
    .pcpuhand   = NULL,
    .next       = SLIST_ENTRY_INITIALIZER,
    .name       = "ARM64",
    .type       = CPUTIMER_INTR_ARM64,
    .prio       = CPUTIMER_INTR_PRIO_VMM,
    .caps       = CPUTIMER_INTR_CAP_PS,
    .priv       = NULL,
};

static sysclock_t
arm64_cputimer_count(void)
{
    return (sysclock_t)arm64_read_cntvct();
}

static void
arm64_timer_intr_reload(struct cputimer_intr *cti __unused, sysclock_t reload)
{
    /* Disable timer first */
    arm64_write_cntv_ctl(GT_CTRL_INT_MASK);
    
    if (reload > 0) {
        /* Set countdown value and enable */
        arm64_write_cntv_tval((int32_t)reload);
        arm64_write_cntv_ctl(GT_CTRL_ENABLE);
    }
}

static void
arm64_timer_intr_enable(struct cputimer_intr *cti __unused)
{
    /* Enable timer IRQ in GIC */
    gic_enable_irq(GIC_VIRT_TIMER_IRQ);
}

static void
arm64_timer_intr_initclock(struct cputimer_intr *cti, boolean_t selected)
{
    if (!selected)
        return;
    
    /* Mask interrupt until first reload */
    arm64_write_cntv_ctl(GT_CTRL_INT_MASK);
}

/*
 * Timer interrupt handler - called from exception vector
 */
void
arm64_timer_intr(struct intrframe *frame)
{
    /* Mask interrupt to acknowledge */
    arm64_write_cntv_ctl(GT_CTRL_INT_MASK);
    
    /* Process system timers */
    pcpu_timer_process_frame(frame);
}

/*
 * Initialize timer subsystem
 */
static void
arm64_timer_init(void *dummy __unused)
{
    uint32_t freq;
    
    /* Get frequency from firmware-programmed register */
    freq = arm64_read_cntfrq();
    if (freq == 0) {
        kprintf("ARM64 timer: CNTFRQ not set by firmware!\n");
        return;
    }
    
    kprintf("ARM64 timer: frequency %u Hz (%u MHz)\n", 
            freq, freq / 1000000);
    
    /* Register cputimer */
    arm64_cputimer.freq = freq;
    cputimer_register(&arm64_cputimer);
    cputimer_select(&arm64_cputimer, 0);
    
    /* Register interrupt timer */
    arm64_cputimer_intr.freq = freq;
    cputimer_intr_register(&arm64_cputimer_intr);
    cputimer_intr_select(&arm64_cputimer_intr, 0);
}

SYSINIT(arm64_timer, SI_BOOT2_CLOCKREG, SI_ORDER_FIRST, arm64_timer_init, NULL);
```

### Exception Vector Changes (locore.s)

Add IRQ handling to the exception vector:

```asm
/*
 * IRQ exception handler (EL1)
 * Called when GIC delivers an interrupt
 */
ENTRY(handle_irq)
    /* Save context */
    sub     sp, sp, #(34 * 8)       /* 32 GP regs + ELR + SPSR */
    stp     x0, x1, [sp, #(0 * 8)]
    stp     x2, x3, [sp, #(2 * 8)]
    /* ... save all registers ... */
    stp     x28, x29, [sp, #(28 * 8)]
    str     x30, [sp, #(30 * 8)]
    mrs     x0, elr_el1
    mrs     x1, spsr_el1
    stp     x0, x1, [sp, #(32 * 8)]
    
    /* Get IRQ number from GIC */
    bl      gic_get_irq
    
    /* Check for spurious interrupt */
    cmp     w0, #1023
    b.eq    1f
    
    /* Save IRQ number */
    mov     w19, w0
    
    /* Check if timer IRQ */
    cmp     w0, #27                 /* GIC_VIRT_TIMER_IRQ */
    b.ne    2f
    
    /* Timer interrupt - pass frame pointer */
    mov     x0, sp
    bl      arm64_timer_intr
    b       3f

2:  /* Unknown IRQ - just EOI it */
    /* Future: dispatch to registered handlers */

3:  /* Send EOI */
    mov     w0, w19
    bl      gic_eoi

1:  /* Restore context and return */
    ldp     x0, x1, [sp, #(32 * 8)]
    msr     elr_el1, x0
    msr     spsr_el1, x1
    ldp     x0, x1, [sp, #(0 * 8)]
    ldp     x2, x3, [sp, #(2 * 8)]
    /* ... restore all registers ... */
    ldp     x28, x29, [sp, #(28 * 8)]
    ldr     x30, [sp, #(30 * 8)]
    add     sp, sp, #(34 * 8)
    eret
END(handle_irq)
```

### systimer.h Changes

Add new constants:

```c
/* In sys/sys/systimer.h */

#define CPUTIMER_ARM64          12

#define CPUTIMER_INTR_ARM64     4
```

### machdep.c Changes

Add GIC initialization early in boot:

```c
/* In initarm() or arm64_init(), after DMAP is set up */

/* Initialize GIC before timer registration */
gic_init();
```

### conf/files Changes

```
platform/arm64/aarch64/gic.c           standard
platform/arm64/aarch64/generic_timer.c standard
```

## Testing Plan

1. Build kernel via arm64-port-testing agent
2. Run QEMU test
3. Expected results:
   - GIC init message: "GIC: initialized (dist=... cpu=...)"
   - Timer init message: "ARM64 timer: frequency ... Hz"
   - Kernel progresses past SI_BOOT2_POST_SMP
   - Timer interrupts fire (scheduler runs)

## Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| DMAP not covering GIC addresses | GIC init fails | GIC at 0x08000000 should be covered by DMAP |
| Timer frequency not set | Timer unusable | QEMU always sets CNTFRQ; panic if zero |
| Exception vector bugs | Kernel hangs/crashes | Test carefully, add debug output |
| Missing context save/restore | Corruption | Follow FreeBSD pattern exactly |

## Future Enhancements

Once basic timer works:
1. Add cpucounter interface for high-resolution timing
2. Implement proper IRQ dispatch with handler registration
3. Add GICv3 support for real hardware
4. Add FDT/ACPI discovery for portable configuration
5. SMP support with per-CPU timer initialization

## References

- FreeBSD `sys/arm/arm/gic.c` - GIC implementation
- FreeBSD `sys/arm/arm/generic_timer.c` - Timer implementation
- ARM Generic Interrupt Controller Architecture Specification
- ARM Architecture Reference Manual - Generic Timer
- DragonFly `sys/platform/pc64/isa/clock.c` - x86 timer for cputimer API

---

*Created: 2026-01-27*
*Last updated: 2026-01-28*
*Status: COMPLETE - Timer and GIC working, kernel boots to mountroot*
