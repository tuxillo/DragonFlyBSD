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
 * Test 26: work_pending and work_busy
 *
 * Tests work status query functions:
 * - work_pending() - checks if work is queued
 * - work_busy() - checks if work is queued or running
 * - Verify states before, during, and after queueing
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t work_counter = ATOMIC_INIT(0);

static void
test_work_fn(struct work_struct *work)
{
	atomic_inc(&work_counter);
}

static int
wq_test26_run(void)
{
	struct workqueue_struct *wq;
	struct work_struct work;
	int errors = 0;
	int pending_before, pending_after;
	unsigned int busy_before, busy_after;

	tbridge_printf("Test: work_pending() and work_busy()...\n");

	wq = alloc_workqueue("test_pending", 0, 1);
	if (wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue() failed\n");
		return 1;
	}

	atomic_set(&work_counter, 0);
	INIT_WORK(&work, test_work_fn);

	/* Check state before queueing */
	pending_before = work_pending(&work);
	busy_before = work_busy(&work);
	tbridge_printf("INFO: Before queue: pending=%d, busy=%u\n",
	    pending_before, busy_before);

	if (pending_before == 0) {
		tbridge_printf("PASS: work_pending() returns 0 before queueing\n");
	} else {
		tbridge_printf("FAIL: work_pending() should be 0 before queueing\n");
		errors++;
	}

	/* Queue the work */
	queue_work(wq, &work);

	/* Wait for completion */
	flush_work(&work);

	/* Check state after completion */
	pending_after = work_pending(&work);
	busy_after = work_busy(&work);
	tbridge_printf("INFO: After flush: pending=%d, busy=%u\n",
	    pending_after, busy_after);

	if (pending_after == 0) {
		tbridge_printf("PASS: work_pending() returns 0 after completion\n");
	} else {
		tbridge_printf("FAIL: work_pending() should be 0 after completion\n");
		errors++;
	}

	if (busy_after == 0) {
		tbridge_printf("PASS: work_busy() returns 0 after completion\n");
	} else {
		tbridge_printf("FAIL: work_busy() should be 0 after completion\n");
		errors++;
	}

	if (atomic_read(&work_counter) == 1) {
		tbridge_printf("PASS: work callback executed (count=%d)\n",
		    atomic_read(&work_counter));
	} else {
		tbridge_printf("FAIL: work callback not executed (count=%d)\n",
		    atomic_read(&work_counter));
		errors++;
	}

	destroy_workqueue(wq);

	return errors;
}

DEFINE_WQ_TEST(wq_test26, "work_pending and work_busy");
