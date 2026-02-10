# i915 Driver Memory Corruption Investigation Report

**Date:** 2026-02-10  
**Issue:** Kernel memory corruption / protection faults during i915 probe on DragonFly BSD VM passthrough  
**Hardware:** Intel Alder Lake-N (UHD Graphics), PCI passthrough to QEMU/KVM

## Executive Summary

This investigation moved from noisy/random crash points to bounded fault domains using deterministic probe gates.

Current status:
- FreeBSD succeeds with the same passthrough setup.
- DragonFly fails with stochastic trap points when ungated.
- Deterministic cutoffs prove early probe setup is stable up to late Step 7 boundaries.
- First unsafe domains are GT MMIO init and later display PPS handling.

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

When probe is forced past earlier GT paths, Step 11 often reaches:
- `drm` error: `PPS state mismatch`
- then page fault (often near null write)

This indicates additional instability in display/PPS readback/verify during bring-up.

## Most Likely Root Cause Class

DragonFly-specific MMIO semantics/ordering mismatch in i915 bring-up, not a single deterministic function bug in all runs.

Most implicated regions:
1. GT MMIO initialization (`intel_gt_init_mmio()` path)
2. Display PPS verification/readback path (`intel_pps` during Step 11)

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

The drm-kmod debug branch currently includes temporary instrumentation/gates in:
- `drivers/gpu/drm/i915/i915_driver.c` (S7/S8/S10)
- `drivers/gpu/drm/i915/display/intel_display_driver.c` (S9/S11)
- `drivers/gpu/drm/i915/gt/intel_gt.c` (GTMMIO)
- `drivers/gpu/drm/i915/gt/uc/intel_guc.c`
- `drivers/gpu/drm/drm_vblank.c`
- `drivers/gpu/drm/i915/display/intel_pps.c`
- `drivers/gpu/drm/i915/gt/intel_ggtt.c`

These are diagnostic controls, not final fixes.

## Revised Next Steps (Focused)

### 1. Keep only two choke-point tracers

Retain only:
- Step 7/GTMMIO boundary tracer
- Step 11/PPS boundary tracer

Remove ancillary debug noise to improve signal quality.

### 2. Produce one minimal GTMMIO stabilization patch

Goal: safe DragonFly behavior through `intel_gt_init_mmio()` without blanket skipping.

Acceptance:
- Step 7/8 complete without panic.

### 3. Produce one minimal PPS stabilization patch

Goal: avoid Step 11 PPS mismatch-induced trap during early display bring-up.

Acceptance:
- Step 11 passes without `PPS state mismatch` + page fault.

### 4. Validate against FreeBSD semantics only for implicated functions

Compare DragonFly vs FreeBSD behavior for:
- GT MMIO read/init helpers
- PPS read/verify helpers
- LinuxKPI MMIO/PCI wrappers touched by those paths

## Files Modified During Investigation

**drm-kmod (6.6-lts-dfly debug branch):**
- `drivers/gpu/drm/drm_drv.c`
- `drivers/gpu/drm/i915/i915_driver.c`
- `drivers/gpu/drm/i915/display/intel_display_driver.c`
- `drivers/gpu/drm/i915/gt/intel_gt.c`
- `drivers/gpu/drm/i915/gt/uc/intel_guc.c`
- `drivers/gpu/drm/drm_vblank.c`
- `drivers/gpu/drm/i915/display/intel_pps.c`
- `drivers/gpu/drm/i915/gt/intel_ggtt.c`

**dragonfly:**
- `doc/i915-startup-protection-fault.md`

## Key Insight

This is no longer an unbounded "random crash" investigation.  
It is bounded to two DragonFly-specific bring-up domains:
- GT MMIO init domain
- PPS/display domain

The remaining work is to convert debug skips into minimal, semantics-correct fixes.
