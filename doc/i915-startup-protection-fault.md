# i915 Driver Memory Corruption Investigation Report

**Date:** 2026-02-11  
**Issue:** Kernel memory corruption / protection faults during i915 probe on DragonFly BSD VM passthrough  
**Hardware:** Intel Alder Lake-N (UHD Graphics), PCI passthrough to QEMU/KVM

## Executive Summary

This investigation moved from noisy/random crash points to bounded fault domains using deterministic probe gates.

Current status:
- FreeBSD succeeds with the same passthrough setup.
- DragonFly no longer shows the earlier softclock jump-to-garbage trap after targeted LinuxKPI fixes.
- The active failure mode shifted from hard trap to intermittent i915 probe lockup/stall.
- Current instability is concentrated in IRQ dispatch setup and eDP/PPS AUX bring-up behavior.

The issue is DragonFly-specific behavior (ordering/semantics), not a fundamental passthrough impossibility.

## Observable Failure Pattern

When unfenced, crashes present as:
- privileged instruction faults and page faults in kernel mode
- random fault RIP and garbage disassembly
- occasional garbled console output

This is consistent with corruption/unsafe MMIO side effects where detonation point varies run-to-run.

## Proven Facts

### 1) FreeBSD baseline is good

On identical VM + GPU passthrough config, FreeBSD loads i915 successfully.  
Therefore, hardware + VM config is not inherently broken.

### 2) Duplicate "stolen memory" line is not double init

Repeated line is syslog/console replay behavior, not two driver init invocations.

### 3) Probe entry bus-master state is not the first trigger

Probe logging shows `PCI_COMMAND` master bit cleared at entry (`master=0`), and forcing `pci_clear_master()` did not remove failures.

### 4) Deterministic Step 7 boundaries

With Step 7 checkpoints (`S7`) and explicit aborts:
- cutoffs at S7.02/S7.03/S7.04/S7.05 abort cleanly
- cutoff at S7.06 and deeper paths can still trigger traps under some configurations

This bounded the earliest unstable behavior to MMIO-heavy regions after basic bridge/uncore setup.

### 5) GT MMIO init is a primary unstable domain

`intel_gt_init_mmio()` instrumentation (`GTMMIO.*`) repeatedly correlated with traps when subpaths were enabled.

Temporary subpath guards (`skip_clock`, `skip_uc`, `skip_sseu`, etc.) materially changed whether probe advanced, confirming this domain is causal.

### 6) Step 11 PPS path is a secondary unstable domain

When probe is forced past earlier GT paths, Step 11 can reach:
- `drm` error: `PPS state mismatch`
- then page fault (often near null write)

This indicates additional instability in display/PPS readback/verify during bring-up, but not always the earliest failing domain.

### 7) IRQ install path is also implicated

In later runs that reached deeper probe steps, crashes were observed around IRQ install,
including immediately after entering `request_irq` from i915 IRQ bring-up.

This adds LinuxKPI IRQ/taskqueue semantics as a first-class suspect alongside GT MMIO.

### 8) Softclock callback trap root cause was identified and fixed

Vmcore analysis proved `softclock_handler` was executing a callout callback pointer that actually
pointed to a `struct hrtimer` object, not executable text.

Concrete bug:
- `callout_reset_sbt()` arguments in `linux_hrtimer.c` were passed in the wrong order for DragonFly.

Fix applied:
- corrected `callout_reset_sbt(..., flags, func, arg)` ordering in both hrtimer scheduling paths.

Result:
- the previous privileged-instruction/page-fault trap class in softclock context is no longer the dominant failure mode.

### 9) Additional concrete LinuxKPI bugs were fixed

- MSI descriptor index direction bug in `linux_pci.c`:
  - fixed `vec = irq - irq_start` (was reversed).
- IRQ registration call signature mismatch in `linux_interrupt.c`:
  - corrected DragonFly `bus_setup_intr()` argument ordering.
- Threaded IRQ null-primary handling in `linux_interrupt.c`:
  - `lkpi_irq_handler()` now handles `handler == NULL` safely when only `thread_handler` is provided.

## Most Likely Root Cause Class

DragonFly LinuxKPI semantic mismatches in interrupt/timer glue were major contributors,
combined with fragile i915 display/eDP bring-up in this passthrough environment.

Most implicated regions (current):
1. LinuxKPI IRQ registration/dispatch path (`lkpi_request_irq`/`bus_setup_intr` integration)
2. LinuxKPI timer/hrtimer callout bridging (now fixed for the softclock callback corruption case)
3. Display eDP/PPS AUX bring-up path (`intel_pps`/`intel_dp`)

## What Is De-Prioritized

- Fundamental passthrough hardware incompatibility (contradicted by FreeBSD)
- Double module init as trigger (not observed)
- Earliest Step 7 setup before MMIO-heavy calls (stable under controlled aborts)

## Investigation Method That Worked

The key progress came from deterministic gates, not passive logging:
- Step-level checkpoints (S7/S8/S9/S11)
- Subfunction checkpoints (GTMMIO, GGTT, vblank, PPS)
- Forced cutoffs (`-ECANCELED`) to bound earliest unsafe operation

This converted random failures into a bounded search window.

## Current Debug State in Tree

The temporary debug-session instrumentation/gates were rolled back.
Current branch now carries targeted stabilization fixes (not broad probe gates).

Current state:
- no active broad S7/S8/S9/S10/S11 probe cutoffs
- LinuxKPI stabilization fixes are present in IRQ/timer glue
- i915 display noise was reduced (`drm_WARN_ON_ONCE` for recurring PPS wakeref warning)
- temporary DragonFly-specific eDP bring-up disable was added to reduce lockup surface while stabilizing passthrough

## Revised Next Steps (Focused)

### 1. Stabilize current no-trap branch

Goal: convert intermittent lockup into deterministic pass/fail without reintroducing broad debug gates.

Acceptance:
- no softclock privileged-instruction trap
- no immediate hard panic during `kldload i915kms`

### 2. Confirm IRQ path behavior after signature/null-primary fixes

Validate that:
- `NULL HANDLER vgapci1` no longer appears
- threaded IRQ paths with NULL primary handlers execute safely

### 3. Keep eDP/PPS path constrained while passthrough is stabilized

Current temporary policy:
- disable eDP bring-up on DragonFly in this passthrough investigation branch.

Follow-up:
- once IRQ stability is confirmed, revisit eDP/PPS with minimal, targeted enablement.

### 4. Add firmware modules after core stability

Use DragonFly-compatible firmware modules (`.ko` registration path), not Linux-style raw blob assumptions, then reevaluate runtime PM and display behavior.

## Files Modified During Investigation (Historical)

**drm-kmod (6.6-lts-dfly debug branch):**
- `drivers/gpu/drm/drm_drv.c`
- `drivers/gpu/drm/i915/i915_driver.c`
- `drivers/gpu/drm/i915/display/intel_display_driver.c`
- `drivers/gpu/drm/i915/display/intel_dp.c`
- `drivers/gpu/drm/i915/gt/intel_gt.c`
- `drivers/gpu/drm/i915/gt/uc/intel_guc.c`
- `drivers/gpu/drm/drm_vblank.c`
- `drivers/gpu/drm/i915/display/intel_pps.c`
- `drivers/gpu/drm/i915/gt/intel_ggtt.c`

**dragonfly:**
- `sys/compat/linuxkpi/common/src/linux_pci.c`
- `sys/compat/linuxkpi/common/src/linux_hrtimer.c`
- `sys/compat/linuxkpi/common/src/linux_work.c`
- `sys/compat/linuxkpi/common/src/linux_interrupt.c`
- `doc/i915-startup-protection-fault.md`

## Key Insight

This is no longer an unbounded "random crash" investigation.  
Multiple concrete LinuxKPI integration bugs have now been identified and fixed.

The remaining work is to finish lockup stabilization in IRQ/eDP paths and then layer firmware modules.
