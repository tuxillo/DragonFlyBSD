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
 * Test 27: delayed_work_pending
 *
 * Tests delayed_work_pending():
 * - Schedule delayed work
 * - Check pending state before delay expires
 * - Wait for completion
 * - Check pending state after completion
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t work_counter = ATOMIC_INIT(0);

static void
test_delayed_work_fn(struct work_struct *work)
{
	atomic_inc(&work_counter);
}

static int
wq_test27_run(void)
{
	struct workqueue_struct *wq;
	struct delayed_work dwork;
	int errors = 0;
	int pending_before, pending_after;

	tbridge_printf("Test: delayed_work_pending()...\n");

	wq = alloc_workqueue("test_delayed_pending", 0, 1);
	if (wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue() failed\n");
		return 1;
	}

	atomic_set(&work_counter, 0);
	INIT_DELAYED_WORK(&dwork, test_delayed_work_fn);

	/* Check state before queueing */
	pending_before = delayed_work_pending(&dwork);
	tbridge_printf("INFO: Before queue: delayed_work_pending=%d\n",
	    pending_before);

	if (pending_before == 0) {
		tbridge_printf("PASS: delayed_work_pending() returns 0 before queueing\n");
	} else {
		tbridge_printf("FAIL: delayed_work_pending() should be 0 before queueing\n");
		errors++;
	}

	/* Schedule delayed work with short delay */
	queue_delayed_work(wq, &dwork, msecs_to_jiffies(10));

	/* Give it time to complete */
	flush_delayed_work(&dwork);

	/* Check state after completion */
	pending_after = delayed_work_pending(&dwork);
	tbridge_printf("INFO: After flush: delayed_work_pending=%d\n",
	    pending_after);

	if (pending_after == 0) {
		tbridge_printf("PASS: delayed_work_pending() returns 0 after completion\n");
	} else {
		tbridge_printf("FAIL: delayed_work_pending() should be 0 after completion\n");
		errors++;
	}

	if (atomic_read(&work_counter) == 1) {
		tbridge_printf("PASS: delayed work callback executed (count=%d)\n",
		    atomic_read(&work_counter));
	} else {
		tbridge_printf("FAIL: delayed work callback not executed (count=%d)\n",
		    atomic_read(&work_counter));
		errors++;
	}

	destroy_workqueue(wq);

	return errors;
}

DEFINE_WQ_TEST(wq_test27, "delayed_work_pending");
