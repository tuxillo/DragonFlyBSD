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
 * Test 23: flush_workqueue
 *
 * Tests flush_workqueue():
 * - Create workqueue
 * - Queue multiple work items
 * - Call flush_workqueue()
 * - Verify all completed
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t work_counter = ATOMIC_INIT(0);

static void
test_work_fn(struct work_struct *work)
{
	atomic_inc(&work_counter);
}

static int
wq_test23_run(void)
{
	struct workqueue_struct *wq;
	struct work_struct *works;
	int i;
	int errors = 0;
	const int num_items = 10;

	tbridge_printf("Test: flush_workqueue() with %d items...\n", num_items);

	wq = alloc_workqueue("test_flush", 0, 4);
	if (wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue() failed\n");
		return 1;
	}

	works = kmalloc(sizeof(*works) * num_items, GFP_KERNEL);
	if (works == NULL) {
		tbridge_printf("FAIL: kmalloc() failed\n");
		destroy_workqueue(wq);
		return 1;
	}

	atomic_set(&work_counter, 0);

	/* Queue multiple work items */
	for (i = 0; i < num_items; i++) {
		INIT_WORK(&works[i], test_work_fn);
		queue_work(wq, &works[i]);
	}

	tbridge_printf("INFO: Queued %d work items\n", num_items);

	/* flush_workqueue waits for all queued work to complete */
	flush_workqueue(wq);

	if (atomic_read(&work_counter) == num_items) {
		tbridge_printf("PASS: All %d work callbacks executed after flush\n",
		    atomic_read(&work_counter));
	} else {
		tbridge_printf("FAIL: Expected %d callbacks, got %d\n",
		    num_items, atomic_read(&work_counter));
		errors++;
	}

	destroy_workqueue(wq);
	kfree(works);

	return errors;
}

DEFINE_WQ_TEST(wq_test23, "flush_workqueue");
