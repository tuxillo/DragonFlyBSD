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
 * Test 05: Multiple work items (50 items)
 *
 * Tests queuing many work items to a multi-threaded workqueue:
 * - Allocate array of work structs
 * - Queue all items
 * - Use drain_workqueue() to wait for all
 * - Verify all callbacks executed
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t work_counter = ATOMIC_INIT(0);

static void
test_work_fn(struct work_struct *work)
{
	atomic_inc(&work_counter);
}

static int
wq_test05_run(void)
{
	struct work_struct *works;
	struct workqueue_struct *wq;
	int i;
	int errors = 0;
	const int num_items = 50;

	tbridge_printf("Test: Multiple work items (%d items)...\n", num_items);

	wq = alloc_workqueue("test_multi", 0, 4);
	if (wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue() failed\n");
		return 1;
	}

	atomic_set(&work_counter, 0);

	works = kmalloc(sizeof(*works) * num_items, GFP_KERNEL);
	if (works == NULL) {
		tbridge_printf("FAIL: kmalloc() failed for work items\n");
		destroy_workqueue(wq);
		return 1;
	}

	for (i = 0; i < num_items; i++) {
		INIT_WORK(&works[i], test_work_fn);
		queue_work(wq, &works[i]);
	}

	drain_workqueue(wq);

	if (atomic_read(&work_counter) == num_items) {
		tbridge_printf("PASS: All %d work callbacks executed\n", atomic_read(&work_counter));
	} else {
		tbridge_printf("FAIL: Expected %d callbacks, got %d\n", num_items, atomic_read(&work_counter));
		errors++;
	}

	destroy_workqueue(wq);
	kfree(works);

	return errors;
}

DEFINE_WQ_TEST(wq_test05, "Multiple work items (50)");
