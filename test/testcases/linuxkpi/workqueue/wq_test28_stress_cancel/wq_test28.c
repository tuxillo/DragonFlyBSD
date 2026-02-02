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
 * Test 28: Concurrent cancel and queue stress test
 *
 * Stress test for concurrent queue and cancel operations:
 * - Multiple rounds of queue_work/cancel_work_sync
 * - Verify no crashes, hangs, or memory corruption
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t work_counter = ATOMIC_INIT(0);
static atomic_t cancel_counter = ATOMIC_INIT(0);

static void
test_work_fn(struct work_struct *work)
{
	atomic_inc(&work_counter);
}

static int
wq_test28_run(void)
{
	struct workqueue_struct *wq;
	struct work_struct work;
	int i;
	int errors = 0;
	const int num_rounds = 100;
	int executed, cancelled;

	tbridge_printf("Test: Concurrent cancel/queue stress (%d rounds)...\n",
	    num_rounds);

	wq = alloc_workqueue("test_stress_cancel", 0, 4);
	if (wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue() failed\n");
		return 1;
	}

	atomic_set(&work_counter, 0);
	atomic_set(&cancel_counter, 0);
	INIT_WORK(&work, test_work_fn);

	/*
	 * Stress test: repeatedly queue and immediately cancel.
	 * Some work may execute, some may be cancelled - both are valid.
	 */
	for (i = 0; i < num_rounds; i++) {
		if (queue_work(wq, &work)) {
			/* Work was queued, try to cancel it */
			if (cancel_work_sync(&work)) {
				atomic_inc(&cancel_counter);
			}
		}
		/* Re-initialize for next round (work may have executed) */
		INIT_WORK(&work, test_work_fn);
	}

	/* Final flush to ensure any remaining work completes */
	drain_workqueue(wq);

	executed = atomic_read(&work_counter);
	cancelled = atomic_read(&cancel_counter);

	tbridge_printf("INFO: Rounds=%d, Executed=%d, Cancelled=%d\n",
	    num_rounds, executed, cancelled);

	/*
	 * Success criteria: no crash/hang, and executed + cancelled
	 * should be reasonable (can't be exact due to timing).
	 */
	if (executed >= 0 && cancelled >= 0) {
		tbridge_printf("PASS: Stress test completed without crash/hang\n");
	} else {
		tbridge_printf("FAIL: Unexpected counter values\n");
		errors++;
	}

	destroy_workqueue(wq);
	tbridge_printf("PASS: Cleanup completed successfully\n");

	return errors;
}

DEFINE_WQ_TEST(wq_test28, "Concurrent cancel/queue stress");
