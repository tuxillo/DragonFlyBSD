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
 * Test 16: Self-requeueing delayed work
 *
 * **KNOWN BUGGY** - Tests delayed work that re-queues itself from callback.
 * This is a stress test for the delayed work implementation.
 *
 * - Callback increments counter and re-queues itself with short delay
 * - Repeats 5 times then stops
 * - NO debug printfs in callback to avoid stack overflow
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t delayed_self_requeue_count = ATOMIC_INIT(0);
static struct delayed_work delayed_requeue_dwork;
static struct workqueue_struct *delayed_requeue_wq;

/*
 * Self-requeueing delayed work callback.
 * IMPORTANT: No tbridge_printf calls here to avoid stack overflow.
 */
static void
test_delayed_self_requeue_fn(struct work_struct *work)
{
	int count = atomic_inc_return(&delayed_self_requeue_count);

	if (count < 5) {
		/* Re-queue with short delay */
		queue_delayed_work(delayed_requeue_wq, &delayed_requeue_dwork, hz / 20);
	}
}

static int
wq_test16_run(void)
{
	int errors = 0;

	tbridge_printf("Test: Self-requeueing delayed work...\n");

	atomic_set(&delayed_self_requeue_count, 0);

	delayed_requeue_wq = alloc_workqueue("test_delay_requeue", 0, 1);
	if (delayed_requeue_wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue() failed\n");
		return 1;
	}

	INIT_DELAYED_WORK(&delayed_requeue_dwork, test_delayed_self_requeue_fn);

	/* Queue initial delayed work with short delay */
	schedule_delayed_work(&delayed_requeue_dwork, hz / 20);

	/* Wait for all iterations (5 * 50ms = ~250ms) */
	/* Use a loop with flush to catch all re-queues */
	while (atomic_read(&delayed_self_requeue_count) < 5) {
		flush_delayed_work(&delayed_requeue_dwork);
		if (atomic_read(&delayed_self_requeue_count) >= 5)
			break;
		DELAY(10000);  /* 10ms */
	}

	if (atomic_read(&delayed_self_requeue_count) == 5) {
		tbridge_printf("PASS: Delayed work re-queued correctly (%d iterations)\n",
			atomic_read(&delayed_self_requeue_count));
	} else {
		tbridge_printf("FAIL: Expected 5 iterations, got %d\n",
			atomic_read(&delayed_self_requeue_count));
		errors++;
	}

	/* Ensure all work is cancelled and drained before destroying */
	cancel_delayed_work_sync(&delayed_requeue_dwork);
	drain_workqueue(delayed_requeue_wq);
	destroy_workqueue(delayed_requeue_wq);

	return errors;
}

DEFINE_WQ_TEST(wq_test16, "Self-requeueing delayed work");
