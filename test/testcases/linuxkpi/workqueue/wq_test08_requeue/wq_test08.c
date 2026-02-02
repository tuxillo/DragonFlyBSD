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
 * Test 08: Work re-queueing (chained work)
 *
 * Tests work that re-queues itself from within its callback:
 * - Work callback increments counter and calls queue_work() on itself
 * - Repeats 5 times then stops
 * - Use flush_work() to wait
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t requeue_count = ATOMIC_INIT(0);
static struct work_struct requeue_work;
static struct workqueue_struct *requeue_wq;

static void
test_requeue_fn(struct work_struct *work)
{
	int count = atomic_inc_return(&requeue_count);
	if (count < 5) {
		/* Re-queue the work for another run */
		queue_work(requeue_wq, work);
	}
}

static int
wq_test08_run(void)
{
	int errors = 0;

	tbridge_printf("Test: Work re-queueing (chained work)...\n");

	atomic_set(&requeue_count, 0);

	requeue_wq = alloc_workqueue("test_requeue", 0, 1);
	if (requeue_wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue() failed\n");
		return 1;
	}

	INIT_WORK(&requeue_work, test_requeue_fn);

	/* Queue initial work */
	queue_work(requeue_wq, &requeue_work);
	tbridge_printf("INFO: Queued initial work\n");

	/* Flush - should complete all 5 iterations */
	flush_work(&requeue_work);

	if (atomic_read(&requeue_count) == 5) {
		tbridge_printf("PASS: Work re-queued correctly (%d iterations)\n", 
			atomic_read(&requeue_count));
	} else {
		tbridge_printf("FAIL: Expected 5 iterations, got %d\n", 
			atomic_read(&requeue_count));
		errors++;
	}

	destroy_workqueue(requeue_wq);
	tbridge_printf("PASS: Work re-queueing test completed\n");

	return errors;
}

DEFINE_WQ_TEST(wq_test08, "Work re-queueing (chained work)");
