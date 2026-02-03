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
 * Test 16: Self-requeueing delayed work (amdgpu idle work pattern)
 *
 * This test mirrors the pattern used in amdgpu VCN/UVD/VCE/JPEG idle work:
 * - Uses schedule_delayed_work() which queues to system_wq
 * - Work handler checks condition and re-queues itself if needed
 * - This is the most common GPU driver pattern for power management
 *
 * Real example from amdgpu_vcn.c:
 *   static void amdgpu_vcn_idle_work_handler(struct work_struct *work) {
 *       if (fences_pending)
 *           schedule_delayed_work(&adev->vcn.idle_work, VCN_IDLE_TIMEOUT);
 *       else
 *           power_gate_vcn();
 *   }
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t idle_work_count = ATOMIC_INIT(0);
static struct delayed_work idle_dwork;

/*
 * Simulates amdgpu idle work handler.
 * Re-queues itself to system_wq via schedule_delayed_work().
 */
static void
test_idle_work_fn(struct work_struct *work)
{
	int count = atomic_inc_return(&idle_work_count);

	/* Simulate: if still busy, re-queue to check again later */
	if (count < 5) {
		schedule_delayed_work(&idle_dwork, hz / 20);  /* 50ms */
	}
	/* else: "hardware idle" - stop re-queueing */
}

static int
wq_test16_run(void)
{
	int errors = 0;
	int timeout_loops = 0;
	const int max_loops = 100;  /* 100 * 10ms = 1 second max */

	tbridge_printf("Test: Self-requeueing delayed work (amdgpu pattern)...\n");

	atomic_set(&idle_work_count, 0);

	INIT_DELAYED_WORK(&idle_dwork, test_idle_work_fn);

	/* Initial schedule - like amdgpu_vcn_sw_init() does */
	schedule_delayed_work(&idle_dwork, hz / 20);  /* 50ms delay */

	/*
	 * Wait for all iterations to complete.
	 * Don't use flush_delayed_work in a tight loop - just poll the counter.
	 * Real drivers typically wait on a different event or just let it run.
	 */
	while (atomic_read(&idle_work_count) < 5 && timeout_loops < max_loops) {
		pause("wqwait", hz / 100);  /* 10ms */
		timeout_loops++;
	}

	if (atomic_read(&idle_work_count) == 5) {
		tbridge_printf("PASS: Idle work completed %d iterations\n",
			atomic_read(&idle_work_count));
	} else {
		tbridge_printf("FAIL: Expected 5 iterations, got %d (timeout=%d)\n",
			atomic_read(&idle_work_count), timeout_loops >= max_loops);
		errors++;
	}

	/* Cleanup - cancel any pending work */
	cancel_delayed_work_sync(&idle_dwork);

	return errors;
}

DEFINE_WQ_TEST(wq_test16, "Self-requeueing delayed work (amdgpu pattern)");
