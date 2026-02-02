# LinuxKPI Test Suite

This document describes the LinuxKPI test modules used to validate the DragonFly BSD
LinuxKPI port. These tests are designed to replicate usage patterns found in the
drm-kmod (DRM/GPU drivers) to ensure compatibility before porting those drivers.

## Overview

The tests use the DragonFly `tbridge` (test bridge) framework and are run via `dfregress`.

```bash
cd /usr/src/test/testcases
dfregress linuxkpi/workqueue/linuxkpi_workqueue.ko
```

---

## Workqueue Tests (`linuxkpi/workqueue/`)

The workqueue subsystem is heavily used by DRM drivers. In drm-kmod:
- `queue_work` / `schedule_work`: 125+ uses
- `cancel_delayed_work_sync`: 82 uses
- `schedule_delayed_work`: 52 uses
- `flush_work`: 52 uses
- `INIT_DELAYED_WORK`: 45 uses

### Implemented Tests

#### Test 1: alloc_workqueue and destroy_workqueue
**Status:** Implemented

Tests basic workqueue lifecycle:
- Create a single-threaded workqueue with `alloc_workqueue("name", 0, 1)`
- Create a multi-threaded workqueue with `alloc_workqueue("name", 0, 4)`
- Destroy both with `destroy_workqueue()`

**drm-kmod relevance:** i915 creates multiple custom workqueues (`dev_priv->wq`,
`dev_priv->unordered_wq`, etc.)

---

#### Test 2: queue_work on custom workqueue
**Status:** Implemented

Tests basic work submission:
- Create workqueue
- Initialize work with `INIT_WORK()`
- Queue with `queue_work()`
- Wait with `flush_work()`
- Verify callback executed

**drm-kmod relevance:** Basic pattern used throughout all DRM drivers.

---

#### Test 3: queue_work on system_wq
**Status:** Implemented

Tests the global `system_wq`:
- Initialize work with `INIT_WORK()`
- Queue with `queue_work(system_wq, &work)`
- Wait with `flush_work()`

**drm-kmod relevance:** Many drivers use `system_wq` for general-purpose work.

---

#### Test 4: schedule_work convenience function
**Status:** Implemented

Tests the `schedule_work()` macro (queues to `system_wq`):
- Initialize work
- Queue with `schedule_work(&work)`
- Verify execution

**drm-kmod relevance:** 50+ uses in drm-kmod.

---

#### Test 5: Multiple work items
**Status:** Implemented

Tests queuing many work items (50) to a multi-threaded workqueue:
- Allocate array of work structs
- Queue all items
- Use `drain_workqueue()` to wait for all
- Verify all callbacks executed
- Check all work items return to IDLE state

**drm-kmod relevance:** DRM drivers frequently queue many work items concurrently.

---

#### Test 6: cancel_work_sync
**Status:** Implemented

Tests synchronous work cancellation:
- Queue work to `system_wq`
- Immediately call `cancel_work_sync()`
- Verify no crash/hang

**drm-kmod relevance:** 18 uses in drm-kmod for cleanup paths.

---

#### Test 7: Sustained work processing
**Status:** Implemented

Tests sequential work processing on a single-threaded workqueue:
- Create with `create_singlethread_workqueue()`
- Queue 20 work items
- Use `drain_workqueue()` to wait
- Verify all executed

**drm-kmod relevance:** amdgpu uses `create_singlethread_workqueue()` for
hotplug and vblank control.

---

#### Test 8: Work re-queueing (chained work)
**Status:** Implemented

Tests work that re-queues itself from within its callback:
- Work callback increments counter and calls `queue_work()` on itself
- Repeats 5 times then stops
- Use `flush_work()` to wait

**drm-kmod relevance:** **Critical pattern.** i915 uses this for:
- `retire_work_handler` - periodic request retirement
- `intel_gt_buffer_pool` - periodic buffer cleanup
- Many other periodic maintenance tasks

---

#### Test 9: Multiple concurrent workqueues
**Status:** Implemented

Tests multiple independent workqueues operating simultaneously:
- Create 3 separate workqueues
- Queue work on each
- Flush all
- Verify all executed

**drm-kmod relevance:** i915 creates multiple workqueues with different purposes
(ordered vs unordered, display vs GPU, etc.)

---

#### Test 10: Per-CPU distribution
**Status:** Implemented

Tests that work is distributed across per-CPU taskqueues:
- Create per-CPU workqueue with `alloc_workqueue("name", 0, 0)`
- Queue `ncpus * 5` work items
- Each callback records which CPU it ran on
- Verify distribution across CPUs

**drm-kmod relevance:** Validates the per-CPU workqueue implementation needed
for parallelism.

---

#### Test 11: Stress test (1000 items)
**Status:** Implemented

Stress test with 1000 work items:
- Queue 1000 items to per-CPU workqueue
- Drain and verify all executed

**drm-kmod relevance:** Validates workqueue stability under load.

---

### Delayed Work Tests

#### Test 12: Basic delayed work
**Status:** Implemented

Tests delayed work fundamentals:
- Initialize with `INIT_DELAYED_WORK()`
- Queue with `schedule_delayed_work(&dwork, delay)`
- Wait with `flush_delayed_work()`
- Verify callback executed after delay

**drm-kmod relevance:** 45 uses of `INIT_DELAYED_WORK`, 52 uses of
`schedule_delayed_work`.

---

#### Test 13: cancel_delayed_work_sync (before firing)
**Status:** Implemented

Tests canceling delayed work before timeout expires:
- Queue delayed work with long delay
- Immediately cancel with `cancel_delayed_work_sync()`
- Verify callback did NOT execute
- Verify return value (true = was pending)

**drm-kmod relevance:** **82 uses** - the most common delayed work operation.
Used for idle timeout patterns in amdgpu VCN/UVD/VCE/JPEG.

---

#### Test 14: cancel_delayed_work_sync (after firing)
**Status:** Implemented

Tests canceling delayed work that already executed:
- Queue delayed work with short delay
- Wait for it to execute
- Call `cancel_delayed_work_sync()`
- Verify return value (false = was not pending)

**drm-kmod relevance:** Cleanup paths must handle both cases.

---

#### Test 15: Delayed work timeout fires
**Status:** Implemented

Tests that delayed work actually fires after timeout:
- Queue with `queue_delayed_work(wq, &dwork, hz/10)` (100ms)
- Sleep longer than delay
- Verify callback executed
- Verify `dwork->timer.expires` was set correctly

**drm-kmod relevance:** Basic delayed work functionality.

---

#### Test 16: Self-requeueing delayed work
**Status:** Implemented

Tests delayed work that re-queues itself (periodic task):
- Callback calls `queue_delayed_work()` on itself
- Repeats N times then stops
- Verify all iterations executed

**drm-kmod relevance:** **Critical pattern.** Used by:
- i915 `retire_work_handler` - periodic cleanup
- amdgpu `vf2pf_work` - periodic VF-to-PF communication
- Many other periodic maintenance tasks

---

#### Test 17: mod_delayed_work
**Status:** Implemented

Tests modifying pending delayed work:
- Queue delayed work with long delay
- Call `mod_delayed_work()` to change delay (shorter or longer)
- Verify work fires at new time
- Test `mod_delayed_work()` on non-pending work (should queue it)

**drm-kmod relevance:** 14 uses. DRM scheduler uses this for timeout management.
i915 uses for hotplug and heartbeat.

---

#### Test 18: Delayed work idle timeout pattern
**Status:** Implemented

Tests the "idle timeout" pattern used by amdgpu power management:
```c
void begin_use() {
    if (!cancel_delayed_work_sync(&idle_work)) {
        // Work wasn't pending, need to power up
        power_up();
    }
}
void end_use() {
    schedule_delayed_work(&idle_work, IDLE_TIMEOUT);
}
```

**drm-kmod relevance:** Used by amdgpu VCN, UVD, VCE, JPEG for power management.

---

#### Test 19: alloc_ordered_workqueue
**Status:** Implemented

Tests ordered (serialized) workqueue:
- Create with `alloc_ordered_workqueue("name", 0)`
- Queue multiple work items
- Verify they execute in order (not concurrently)

**drm-kmod relevance:** 7 uses. i915 uses ordered workqueues for serialization
(`dev_priv->wq`, display modeset workqueue).

---

#### Test 20: system_unbound_wq
**Status:** Implemented

Tests the global `system_unbound_wq`:
- Queue work with `queue_work(system_unbound_wq, &work)`
- Verify execution

**drm-kmod relevance:** Heavily used for work that shouldn't be CPU-bound
(fence handling, VMA operations, atomic commits).

---

#### Test 21: system_highpri_wq
**Status:** Implemented

Tests the global `system_highpri_wq`:
- Queue work
- Verify execution

**drm-kmod relevance:** Used for IRQ-triggered work, urgent tasks
(GuC log flush, display commits).

---

#### Test 22: system_long_wq
**Status:** Implemented

Tests the global `system_long_wq`:
- Queue work
- Verify execution

**drm-kmod relevance:** Used for long-running operations
(MST topology discovery, writeback).

---

#### Test 23: flush_workqueue
**Status:** Implemented

Tests flushing an entire workqueue:
- Queue multiple work items
- Call `flush_workqueue(wq)` (not `flush_work` on individual items)
- Verify all completed

**drm-kmod relevance:** 11 uses. Used when all pending work must complete.

---

#### Test 24: WQ_HIGHPRI flag
**Status:** Implemented

Tests high-priority workqueue creation:
- Create with `alloc_workqueue("name", WQ_HIGHPRI, 0)`
- Queue work
- Verify execution

**drm-kmod relevance:** TTM and i915 display use `WQ_HIGHPRI`.

---

#### Test 25: WQ_UNBOUND flag
**Status:** Implemented

Tests unbound workqueue creation:
- Create with `alloc_workqueue("name", WQ_UNBOUND, 0)`
- Queue work
- Verify execution

**drm-kmod relevance:** i915 flip workqueue uses `WQ_HIGHPRI | WQ_UNBOUND`.

---

#### Test 26: Work pending/busy checks
**Status:** Implemented

Tests `work_pending()` and `work_busy()`:
- Check state before queueing (should be false)
- Queue work
- Check state while queued (should be true/busy)
- After completion, check state (should be false)

**drm-kmod relevance:** Used for conditional queueing patterns.

---

#### Test 27: delayed_work_pending
**Status:** Implemented

Tests `delayed_work_pending()`:
- Check before queueing
- Queue delayed work
- Check while pending
- After firing, check again

**drm-kmod relevance:** Used for conditional scheduling.

---

#### Test 28: Concurrent cancel and queue
**Status:** Implemented

Stress test for race conditions:
- Multiple threads: some queueing work, some canceling
- Verify no crashes, hangs, or memory corruption

**drm-kmod relevance:** Real drivers have concurrent access to work items.

---

## Future Test Modules

### RCU Tests (`linuxkpi/rcu/`)
**Status:** Not started

Test `call_rcu()`, `synchronize_rcu()`, `rcu_read_lock/unlock()`, etc.

### Timer Tests (`linuxkpi/timer/`)
**Status:** Not started

Test `timer_setup()`, `mod_timer()`, `del_timer_sync()`, etc.

### Memory Tests (`linuxkpi/memory/`)
**Status:** Not started

Test `kmalloc()`, `kfree()`, `vmalloc()`, `dma_alloc_coherent()`, etc.

### Sync Tests (`linuxkpi/sync/`)
**Status:** Not started

Test `mutex_lock()`, `spin_lock()`, `wait_event()`, `complete()`, etc.

---

## Running Tests

```bash
# Build and run single test
cd /usr/src/test/testcases
dfregress linuxkpi/workqueue/linuxkpi_workqueue.ko

# View test output
cat /var/run/dfregress/linuxkpi_workqueue.out
```

## References

- drm-kmod: https://github.com/freebsd/drm-kmod
- LinuxKPI source: `sys/compat/linuxkpi/`
- Test source: `test/testcases/linuxkpi/`
