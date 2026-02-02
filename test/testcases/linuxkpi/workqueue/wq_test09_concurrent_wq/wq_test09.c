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
 * Test 09: Multiple concurrent workqueues
 *
 * Tests multiple independent workqueues operating simultaneously:
 * - Create 3 separate workqueues
 * - Queue work on each
 * - Flush all
 * - Verify all executed
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t work_counter = ATOMIC_INIT(0);

static void
test_work_fn(struct work_struct *work)
{
	atomic_inc(&work_counter);
}

static int
wq_test09_run(void)
{
	struct workqueue_struct *wq1, *wq2, *wq3;
	struct work_struct work1, work2, work3;
	int errors = 0;

	tbridge_printf("Test: Multiple concurrent workqueues...\n");

	/* Create 3 workqueues */
	wq1 = alloc_workqueue("test_wq1", 0, 2);
	wq2 = alloc_workqueue("test_wq2", 0, 2);
	wq3 = alloc_workqueue("test_wq3", 0, 2);

	if (wq1 == NULL || wq2 == NULL || wq3 == NULL) {
		tbridge_printf("FAIL: alloc_workqueue() failed\n");
		if (wq1) destroy_workqueue(wq1);
		if (wq2) destroy_workqueue(wq2);
		if (wq3) destroy_workqueue(wq3);
		return 1;
	}

	atomic_set(&work_counter, 0);

	/* Queue work on all 3 workqueues */
	INIT_WORK(&work1, test_work_fn);
	INIT_WORK(&work2, test_work_fn);
	INIT_WORK(&work3, test_work_fn);

	queue_work(wq1, &work1);
	queue_work(wq2, &work2);
	queue_work(wq3, &work3);

	tbridge_printf("INFO: Queued work on 3 workqueues\n");

	/* Flush all work */
	flush_work(&work1);
	flush_work(&work2);
	flush_work(&work3);

	if (atomic_read(&work_counter) == 3) {
		tbridge_printf("PASS: All work completed on 3 workqueues (count=%d)\n", 
			atomic_read(&work_counter));
	} else {
		tbridge_printf("FAIL: Expected 3 callbacks, got %d\n", 
			atomic_read(&work_counter));
		errors++;
	}

	destroy_workqueue(wq1);
	destroy_workqueue(wq2);
	destroy_workqueue(wq3);
	tbridge_printf("PASS: All workqueues destroyed\n");

	return errors;
}

DEFINE_WQ_TEST(wq_test09, "Multiple concurrent workqueues");
