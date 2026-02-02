/*-
 * Copyright (c) 2025
 *	The DragonFly Project.  All rights reserved.
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
 * Test 31: Custom workqueue self-requeue (i915 pattern)
 *
 * This test mirrors the pattern used in i915 for heartbeat and retire work:
 *
 * Real example from intel_gt_requests.c:
 *   static void retire_work_handler(struct work_struct *work) {
 *       // Queue first, then work (interesting pattern!)
 *       queue_delayed_work(gt->i915->unordered_wq, &gt->requests.retire_work, HZ);
 *       retire_requests(gt);
 *   }
 *
 * Also from intel_gt_buffer_pool.c:
 *   static void pool_free_work(struct work_struct *work) {
 *       if (pool_free_older_than(pool, jiffies - HZ))
 *           queue_delayed_work(pool->gt->i915->unordered_wq, &pool->work, HZ);
 *   }
 *
 * Key differences from amdgpu (test 16):
 * - Uses a custom/driver-specific workqueue, not system_wq
 * - Uses queue_delayed_work() instead of schedule_delayed_work()
 * - i915 sometimes queues first, then does work (to minimize latency)
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t heartbeat_count = ATOMIC_INIT(0);
static struct delayed_work heartbeat_dwork;
static struct workqueue_struct *i915_wq;  /* Simulates i915->unordered_wq */

/*
 * Simulates i915 heartbeat/retire work handler.
 * Re-queues itself to a custom workqueue via queue_delayed_work().
 */
static void
test_heartbeat_fn(struct work_struct *work)
{
	int count = atomic_inc_return(&heartbeat_count);

	/*
	 * i915 pattern: queue first, then work
	 * This ensures next heartbeat is scheduled even if work takes time
	 */
	if (count < 5) {
		queue_delayed_work(i915_wq, &heartbeat_dwork, hz / 20);  /* 50ms */
	}

	/* Simulate doing actual work after scheduling next iteration */
	DELAY(1000);  /* 1ms of "work" */
}

static int
wq_test31_run(void)
{
	int errors = 0;
	int timeout_loops = 0;
	const int max_loops = 100;  /* 1 second max */

	tbridge_printf("Test: Custom workqueue self-requeue (i915 pattern)...\n");

	/* Create custom workqueue like i915 does */
	i915_wq = alloc_workqueue("i915_unordered", WQ_UNBOUND, 0);
	if (i915_wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue() failed\n");
		return 1;
	}

	atomic_set(&heartbeat_count, 0);
	INIT_DELAYED_WORK(&heartbeat_dwork, test_heartbeat_fn);

	/* Initial queue - like i915 does during init */
	queue_delayed_work(i915_wq, &heartbeat_dwork, hz / 20);

	/* Wait for all iterations */
	while (atomic_read(&heartbeat_count) < 5 && timeout_loops < max_loops) {
		DELAY(10000);  /* 10ms */
		timeout_loops++;
	}

	if (atomic_read(&heartbeat_count) == 5) {
		tbridge_printf("PASS: Heartbeat completed %d iterations\n",
			atomic_read(&heartbeat_count));
	} else {
		tbridge_printf("FAIL: Expected 5 iterations, got %d (timeout=%d)\n",
			atomic_read(&heartbeat_count), timeout_loops >= max_loops);
		errors++;
	}

	/*
	 * Test 2: Verify flush_delayed_work works on custom workqueue
	 */
	tbridge_printf("  Subtest 2: flush_delayed_work on custom wq...\n");
	atomic_set(&heartbeat_count, 0);

	/* Queue work */
	queue_delayed_work(i915_wq, &heartbeat_dwork, 0);  /* immediate */

	/* Flush should wait for completion */
	flush_delayed_work(&heartbeat_dwork);

	if (atomic_read(&heartbeat_count) >= 1) {
		tbridge_printf("  PASS: flush_delayed_work completed\n");
	} else {
		tbridge_printf("  FAIL: flush_delayed_work didn't wait\n");
		errors++;
	}

	/* Wait for any remaining iterations from test 2's self-requeue */
	while (atomic_read(&heartbeat_count) < 5 && timeout_loops < max_loops * 2) {
		DELAY(10000);
		timeout_loops++;
	}

	/* Cleanup */
	cancel_delayed_work_sync(&heartbeat_dwork);
	drain_workqueue(i915_wq);
	destroy_workqueue(i915_wq);

	return errors;
}

DEFINE_WQ_TEST(wq_test31, "Custom workqueue self-requeue (i915 pattern)");
