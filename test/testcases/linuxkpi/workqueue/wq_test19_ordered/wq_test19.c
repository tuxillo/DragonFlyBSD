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
 * Test 19: alloc_ordered_workqueue
 *
 * Tests ordered workqueue serial execution:
 * - Create ordered workqueue (single-threaded, FIFO ordering)
 * - Queue multiple work items
 * - Verify they execute in strict FIFO order
 */

#include "../linuxkpi_workqueue_common.h"

/* Ordered workqueue test - verify serial execution */
static atomic_t ordered_seq = ATOMIC_INIT(0);
static atomic_t ordered_last_seen = ATOMIC_INIT(0);
static atomic_t ordered_errors = ATOMIC_INIT(0);

/* Ordered workqueue callback - records sequence and checks ordering */
static void
test_ordered_work_fn(struct work_struct *work)
{
	int my_seq = atomic_inc_return(&ordered_seq);
	int last = atomic_read(&ordered_last_seen);

	/* In ordered workqueue, we should see monotonically increasing sequence */
	if (my_seq != last + 1) {
		atomic_inc(&ordered_errors);
	}
	atomic_set(&ordered_last_seen, my_seq);

	/* Small delay to increase chance of catching ordering issues */
	DELAY(1000);  /* 1ms */
}

static int
wq_test19_run(void)
{
	struct workqueue_struct *wq;
	struct work_struct *works;
	int i;
	int errors = 0;
	const int num_items = 20;

	tbridge_printf("Test: alloc_ordered_workqueue...\n");

	wq = alloc_ordered_workqueue("test_ordered", 0);
	if (wq == NULL) {
		tbridge_printf("FAIL: alloc_ordered_workqueue() failed\n");
		return 1;
	}

	atomic_set(&ordered_seq, 0);
	atomic_set(&ordered_last_seen, 0);
	atomic_set(&ordered_errors, 0);

	works = kmalloc(sizeof(*works) * num_items, GFP_KERNEL);
	if (works == NULL) {
		tbridge_printf("FAIL: kmalloc() failed\n");
		destroy_workqueue(wq);
		return 1;
	}

	/* Queue all items - they should execute in order */
	for (i = 0; i < num_items; i++) {
		INIT_WORK(&works[i], test_ordered_work_fn);
		queue_work(wq, &works[i]);
	}

	drain_workqueue(wq);

	if (atomic_read(&ordered_seq) == num_items) {
		tbridge_printf("PASS: All %d work items executed\n", num_items);
	} else {
		tbridge_printf("FAIL: Expected %d, got %d\n", num_items, atomic_read(&ordered_seq));
		errors++;
	}

	if (atomic_read(&ordered_errors) == 0) {
		tbridge_printf("PASS: Work executed in order (no ordering errors)\n");
	} else {
		tbridge_printf("FAIL: %d ordering errors detected\n", atomic_read(&ordered_errors));
		errors++;
	}

	destroy_workqueue(wq);
	kfree(works);

	return errors;
}

DEFINE_WQ_TEST(wq_test19, "alloc_ordered_workqueue");
