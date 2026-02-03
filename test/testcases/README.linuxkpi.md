# LinuxKPI Test Suite

This document describes the LinuxKPI test modules used to validate the DragonFly BSD
LinuxKPI port. These tests are designed to replicate usage patterns found in the
drm-kmod (DRM/GPU drivers) to ensure compatibility before porting those drivers.

## Overview

The tests use the DragonFly `tbridge` (test bridge) framework and are run via `dfregress`.

```bash
cd /usr/src/test/testcases
dfregress -r linuxkpi_workqueue.run
```

---

## Workqueue Tests (`linuxkpi/workqueue/`)

The workqueue subsystem is heavily used by DRM drivers. In drm-kmod:
- `queue_work` / `schedule_work`: 125+ uses
- `cancel_delayed_work_sync`: 82 uses
- `schedule_delayed_work`: 52 uses
- `flush_work`: 52 uses
- `INIT_DELAYED_WORK`: 45 uses

### Test Organization

Tests are split into individual kernel modules for isolation. Each test is in its
own directory (`wq_testNN_name/`) containing:
- `wq_testNN.c` - Test source
- `Makefile` - Build configuration

This allows identifying exactly which test causes a crash (the loaded module name
appears in panic messages).

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

#### Test 16: Self-requeueing delayed work (amdgpu pattern)
**Status:** Implemented

Tests delayed work that re-queues itself using `schedule_delayed_work()`:
- Callback calls `schedule_delayed_work()` on itself (queues to system_wq)
- Repeats 5 times then stops
- Verify all iterations executed

**drm-kmod relevance:** **Critical pattern.** Used by:
- amdgpu VCN/UVD/VCE/JPEG idle work handlers
- amdgpu `vf2pf_work` - periodic VF-to-PF communication
- radeon dynpm idle work

Real example from `amdgpu_vcn.c`:
```c
static void amdgpu_vcn_idle_work_handler(struct work_struct *work) {
    if (fences_pending)
        schedule_delayed_work(&adev->vcn.idle_work, VCN_IDLE_TIMEOUT);
    else
        power_gate_vcn();
}
```

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

#### Test 29: cancel_delayed_work_sync return value (amdgpu ring pattern)
**Status:** Implemented

Tests that `cancel_delayed_work_sync()` returns the correct value:
- Returns `true` if work was pending and cancelled
- Returns `false` if work was not pending (already ran or never queued)

Tests the amdgpu `ring_begin_use()` pattern:
```c
void amdgpu_uvd_ring_begin_use(struct amdgpu_ring *ring) {
    set_clocks = !cancel_delayed_work_sync(&adev->uvd.idle_work);
    if (set_clocks) {
        // Work wasn't pending, need to power up
        amdgpu_device_ip_set_powergating_state(..., AMD_PG_STATE_UNGATE);
    }
}
```

**drm-kmod relevance:** The return value is semantically important for
power management race avoidance.

---

#### Test 30: queue_delayed_work with delay=0 (hotplug pattern)
**Status:** Implemented

Tests immediate delayed work execution:
- `schedule_delayed_work(&dwork, 0)` means "queue for immediate execution"
- Work runs ASAP on workqueue thread, not synchronously
- Tests rapid-fire hotplug-style events

Real example from `amdgpu_irq.c`:
```c
static irqreturn_t amdgpu_irq_handler(int irq, void *arg) {
    schedule_delayed_work(&adev->hotplug_work, 0);  // delay=0!
    return IRQ_HANDLED;
}
```

**drm-kmod relevance:** Common pattern for hotplug handlers.

---

#### Test 31: Custom workqueue self-requeue (i915 pattern)
**Status:** Implemented

Tests delayed work that re-queues itself using `queue_delayed_work()` with
a custom workqueue (differs from test 16 which uses `system_wq`):
- Uses `WQ_UNBOUND` custom workqueue (like `i915->unordered_wq`)
- Uses `queue_delayed_work()` instead of `schedule_delayed_work()`
- Tests i915's "queue first, work later" pattern for reduced latency

Real example from `intel_gt_requests.c`:
```c
static void retire_work_handler(struct work_struct *work) {
    // Queue first, then work (interesting pattern!)
    queue_delayed_work(gt->i915->unordered_wq, &gt->requests.retire_work, HZ);
    retire_requests(gt);
}
```

**drm-kmod relevance:** i915 heartbeat, retire work, buffer pool cleanup.

---

## i915-Specific Patterns (Workqueue Tests 32–44)

The following patterns are used by i915 and are covered by workqueue tests 32–44.
These are prioritized by how critical they are for i915 functionality.

### High Priority (Blocks i915 functionality)

#### Test 32: INIT_DELAYED_WORK_ONSTACK
**Status:** Implemented

Tests stack-allocated delayed work:
```c
INIT_DELAYED_WORK_ONSTACK(&w->work, intel_wedge_me);
queue_delayed_work(gt->i915->unordered_wq, &w->work, timeout);
// ... do stuff ...
cancel_delayed_work_sync(&w->work);
destroy_delayed_work_on_stack(&w->work);
```

**drm-kmod relevance:** GPU wedging timeout in `intel_reset.c`.

---

#### Test 33: mod_delayed_work on executing work
**Status:** Implemented

Tests `mod_delayed_work()` called on work that is currently executing:
```c
// Heartbeat reschedules itself while potentially still running
mod_delayed_work(system_highpri_wq, &engine->heartbeat.work, delay + 1);
```

Test 17 covers basic `mod_delayed_work` but NOT this specific edge case.

**drm-kmod relevance:** Engine heartbeat mechanism in `intel_engine_heartbeat.c`.

---

#### Test 36: cancel_delayed_work (non-sync) conditional cleanup
**Status:** Implemented

Tests using `cancel_delayed_work()` (non-sync) return value for conditional cleanup:
```c
void intel_engine_park_heartbeat(struct intel_engine_cs *engine) {
    if (cancel_delayed_work(&engine->heartbeat.work))
        i915_request_put(fetch_and_zero(&engine->heartbeat.systole));
}
```

Only releases resource if work was actually cancelled (was pending).

**drm-kmod relevance:** Resource cleanup in heartbeat parking.

---

#### Test 42: cancel_delayed_work_sync loop pattern
**Status:** Implemented

Tests looping until `cancel_delayed_work_sync()` returns false:
```c
void intel_gt_flush_buffer_pool(struct intel_gt *gt) {
    do {
        while (pool_free_older_than(pool, 0))
            ;
    } while (cancel_delayed_work_sync(&pool->work));
}
```

Needed when work reschedules itself and you need to ensure it's fully stopped.

**drm-kmod relevance:** Buffer pool cleanup in `intel_gt_buffer_pool.c`.

---

### Medium Priority (Important for stability)

#### Test 34: queue_rcu_work
**Status:** Implemented

Tests RCU-delayed work queueing:
```c
queue_rcu_work(ve->context.engine->i915->unordered_wq, &ve->rcu);
```

**drm-kmod relevance:** Virtual engine cleanup in execlists.

---

#### Test 35: Iterative drain with RCU barrier
**Status:** Implemented

Tests the GEM shutdown pattern:
```c
void i915_gem_drain_workqueue(struct drm_i915_private *i915) {
    for (i = 0; i < 3; i++) {
        flush_workqueue(i915->wq);
        rcu_barrier();
        i915_gem_drain_freed_objects(i915);
    }
    drain_workqueue(i915->wq);
}
```

**drm-kmod relevance:** GEM shutdown, catches recursive RCU-delayed work.

---

#### Test 37: Conditional sync vs non-sync cancel
**Status:** Implemented

Tests choosing between sync/non-sync cancel based on lock state:
```c
if (mutex_is_locked(&guc_to_gt(guc)->reset.mutex) ||
    test_bit(I915_RESET_BACKOFF, &guc_to_gt(guc)->reset.flags))
    cancel_delayed_work(&guc->timestamp.work);  // Non-sync to avoid deadlock
else
    cancel_delayed_work_sync(&guc->timestamp.work);  // Sync when safe
```

**drm-kmod relevance:** Avoiding deadlocks during GPU reset.

---

#### Test 41: flush_delayed_work vs flush_work distinction
**Status:** Implemented

Tests that `flush_delayed_work()` waits for both timer AND work execution,
while `flush_work()` only waits for work execution:
```c
flush_work(&engine->retire_work);
flush_delayed_work(&engine->wakeref.work);
```

**drm-kmod relevance:** Proper synchronization semantics.

---

### Lower Priority (Optimization/edge cases)

#### Test 38: WQ_HIGHPRI | WQ_UNBOUND combined flags
**Status:** Implemented

Tests combined workqueue flags:
```c
i915->display.wq.flip = alloc_workqueue("i915_flip",
    WQ_HIGHPRI | WQ_UNBOUND, WQ_UNBOUND_MAX_ACTIVE);
```

Tests 24/25 test these flags separately but not in combination.

**drm-kmod relevance:** Low-latency page flips.

---

#### Test 39: Gated work queueing
**Status:** Implemented

Tests checking a condition before queueing:
```c
static bool mod_delayed_detection_work(...) {
    lockdep_assert_held(&i915->irq_lock);
    if (!detection_work_enabled(i915))
        return false;
    return mod_delayed_work(i915->unordered_wq, work, delay);
}
```

**drm-kmod relevance:** Preventing work submission during suspend.

---

#### Test 40: Work queuing from IRQ context
**Status:** Implemented

Tests queueing to `system_unbound_wq` from IRQ/softirq context:
```c
// In fence callback (potentially IRQ context):
if (ref->flags & I915_ACTIVE_RETIRE_SLEEPS) {
    queue_work(system_unbound_wq, &ref->work);
    return;
}
```

**drm-kmod relevance:** Deferred work from fence callbacks.

---

#### Test 43: Multiple workqueue flush sequence
**Status:** Implemented

Tests correct ordering when flushing multiple workqueues:
```c
void intel_display_driver_remove(struct drm_i915_private *i915) {
    flush_workqueue(i915->display.wq.flip);
    flush_workqueue(i915->display.wq.modeset);
    // ... later ...
    flush_workqueue(i915->unordered_wq);
}
```

**drm-kmod relevance:** Proper shutdown sequencing.

---

#### Test 44: Reference-counted work
**Status:** Implemented

Tests work that holds a reference to prevent use-after-free:
```c
drm_connector_get(&connector->base);
queue_work(i915->unordered_wq, &hdcp->prop_work);
// Work function must call drm_connector_put()
```

**drm-kmod relevance:** HDCP work, prevents use-after-free.

---

## i915 Workqueue Summary

i915 creates these workqueues:
- `i915->wq` - `alloc_ordered_workqueue("i915", 0)` - main ordered workqueue
- `i915->display.hotplug.dp_wq` - `alloc_ordered_workqueue("i915-dp", 0)` - DP hotplug
- `i915->unordered_wq` - `alloc_workqueue("i915-unordered", 0, 0)` - unordered work
- `i915->display.wq.modeset` - `alloc_ordered_workqueue("i915_modeset", 0)` - modeset
- `i915->display.wq.flip` - `alloc_workqueue("i915_flip", WQ_HIGHPRI | WQ_UNBOUND, ...)` - page flips

i915 uses these system workqueues:
- `system_highpri_wq` - heartbeat, GuC timestamp ping, GuC log flush
- `system_unbound_wq` - i915_active, i915_sw_fence_work, GuC submission, PXP, display power

---

## Known LinuxKPI Issues

No known workqueue issues at this time. The self-requeueing delayed work pattern
used by amdgpu and i915 is now covered by tests 16 and 31 and passes consistently.

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
# Build all workqueue tests
cd /usr/src/test/testcases/linuxkpi/workqueue
for d in wq_test*/; do (cd "$d" && make); done

# Run all workqueue tests
cd /usr/src/test/testcases
dfregress -r linuxkpi_workqueue.run

# Run a single test manually
kldload linuxkpi/workqueue/wq_test16_delayed_requeue/wq_test16.ko
dmesg | tail -20
kldunload wq_test16

# View test output
cat /var/run/dfregress/wq_test16.out
```

## References

- drm-kmod: https://github.com/freebsd/drm-kmod
- LinuxKPI source: `sys/compat/linuxkpi/`
- Test source: `test/testcases/linuxkpi/`
