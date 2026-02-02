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
 * Test 30: queue_delayed_work() with delay=0 (hotplug pattern)
 *
 * This test mirrors the pattern used in amdgpu/radeon hotplug handlers:
 *
 * Real example from amdgpu_irq.c:
 *   static irqreturn_t amdgpu_irq_handler(int irq, void *arg) {
 *       schedule_delayed_work(&adev->hotplug_work, 0);  // delay=0!
 *       return IRQ_HANDLED;
 *   }
 *
 * Also from radeon_irq_kms.c:
 *   void radeon_irq_kms_pflip_irq_get(struct radeon_device *rdev, int crtc) {
 *       schedule_delayed_work(&rdev->hotplug_work, 0);
 *   }
 *
 * Using delay=0 means "queue for immediate execution" - the work runs
 * as soon as possible on the workqueue thread, not synchronously.
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t work_executed = ATOMIC_INIT(0);
static struct delayed_work hotplug_dwork;
static struct timeval queue_time, exec_time;

static void
test_hotplug_fn(struct work_struct *work)
{
	microtime(&exec_time);
	atomic_inc(&work_executed);
}

static int
wq_test30_run(void)
{
	int errors = 0;
	long elapsed;

	tbridge_printf("Test: queue_delayed_work with delay=0 (hotplug pattern)...\n");

	INIT_DELAYED_WORK(&hotplug_dwork, test_hotplug_fn);

	/*
	 * Test 1: schedule_delayed_work with delay=0
	 */
	tbridge_printf("  Subtest 1: schedule_delayed_work(&dwork, 0)...\n");
	atomic_set(&work_executed, 0);

	microtime(&queue_time);
	schedule_delayed_work(&hotplug_dwork, 0);

	/* Wait for execution */
	flush_delayed_work(&hotplug_dwork);

	if (atomic_read(&work_executed) == 1) {
		elapsed = wq_elapsed_ms(&queue_time, &exec_time);
		tbridge_printf("  PASS: work executed (latency: %ld ms)\n", elapsed);
		
		/* With delay=0, should execute very quickly (< 100ms typically) */
		if (elapsed > 500) {
			tbridge_printf("  WARN: unexpectedly high latency for delay=0\n");
		}
	} else {
		tbridge_printf("  FAIL: work did not execute\n");
		errors++;
	}

	/*
	 * Test 2: Multiple hotplug-style events in rapid succession
	 * Real drivers may get multiple interrupts quickly
	 */
	tbridge_printf("  Subtest 2: Rapid hotplug events...\n");
	atomic_set(&work_executed, 0);

	/* Fire 5 "hotplug events" rapidly */
	schedule_delayed_work(&hotplug_dwork, 0);
	schedule_delayed_work(&hotplug_dwork, 0);
	schedule_delayed_work(&hotplug_dwork, 0);
	schedule_delayed_work(&hotplug_dwork, 0);
	schedule_delayed_work(&hotplug_dwork, 0);

	/* Wait for completion */
	flush_delayed_work(&hotplug_dwork);
	DELAY(10000);  /* 10ms extra */
	flush_delayed_work(&hotplug_dwork);

	/*
	 * Note: Depending on timing, we may get 1-5 executions.
	 * If work is already pending, schedule_delayed_work returns false
	 * and doesn't re-queue. If work already started, it may queue again.
	 * The important thing is at least 1 execution happened.
	 */
	if (atomic_read(&work_executed) >= 1) {
		tbridge_printf("  PASS: work executed %d time(s) for 5 rapid queues\n",
			atomic_read(&work_executed));
	} else {
		tbridge_printf("  FAIL: work did not execute at all\n");
		errors++;
	}

	/*
	 * Test 3: queue_delayed_work with explicit workqueue and delay=0
	 */
	tbridge_printf("  Subtest 3: queue_delayed_work(system_wq, &dwork, 0)...\n");
	atomic_set(&work_executed, 0);

	queue_delayed_work(system_wq, &hotplug_dwork, 0);

	flush_delayed_work(&hotplug_dwork);

	if (atomic_read(&work_executed) == 1) {
		tbridge_printf("  PASS: work executed on explicit system_wq\n");
	} else {
		tbridge_printf("  FAIL: work did not execute\n");
		errors++;
	}

	/* Final cleanup */
	cancel_delayed_work_sync(&hotplug_dwork);

	return errors;
}

DEFINE_WQ_TEST(wq_test30, "queue_delayed_work with delay=0");
