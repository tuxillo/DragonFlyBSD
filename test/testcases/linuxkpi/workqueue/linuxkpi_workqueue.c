/*-
 * Copyright (c) 2026
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

/* kconfig.h is force-included by compiler flags */

#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/tbridge.h>
#include <sys/thread.h>

/* LinuxKPI headers */
#include <linux/workqueue.h>
#include <linux/gfp.h>
#include <linux/atomic.h>

#include <dfregress.h>

/* Test counters - atomic to avoid race conditions */
static atomic_t work_counter = ATOMIC_INIT(0);
static atomic_t work_done = ATOMIC_INIT(0);

/* Simple work function */
static void test_work_fn(struct work_struct *work)
{
	atomic_inc(&work_counter);
}

/* Work function - just count without delay */
static void test_work_count_fn(struct work_struct *work)
{
	atomic_inc(&work_done);
}

/* Test 1: Basic alloc/destroy workqueue */
static int test_alloc_workqueue(void)
{
	struct workqueue_struct *wq;
	int errors = 0;

	tbridge_printf("Test 1: alloc_workqueue() and destroy_workqueue()...\n");

	/* Test single-threaded workqueue */
	wq = alloc_workqueue("test_wq", 0, 1);
	if (wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue() returned NULL\n");
		errors++;
	} else {
		tbridge_printf("PASS: alloc_workqueue() created single-thread workqueue\n");
		destroy_workqueue(wq);
		tbridge_printf("PASS: destroy_workqueue() completed\n");
	}

	/* Test multi-threaded workqueue */
	wq = alloc_workqueue("test_wq_mt", 0, 4);
	if (wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue(multi-thread) returned NULL\n");
		errors++;
	} else {
		tbridge_printf("PASS: alloc_workqueue(multi-thread) created workqueue\n");
		destroy_workqueue(wq);
		tbridge_printf("PASS: destroy_workqueue(multi-thread) completed\n");
	}

	return errors;
}

/* Test 2: Basic queue_work and flush_work */
static int test_queue_work(void)
{
	struct workqueue_struct *wq;
	struct work_struct work;
	int errors = 0;

	tbridge_printf("\nTest 2: queue_work() and flush_work()...\n");

	wq = alloc_workqueue("test_wq", 0, 1);
	if (wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue() failed\n");
		return 1;
	}

	atomic_set(&work_counter, 0);
	INIT_WORK(&work, test_work_fn);

	if (queue_work(wq, &work)) {
		tbridge_printf("PASS: queue_work() queued successfully\n");
	} else {
		tbridge_printf("FAIL: queue_work() returned false\n");
		errors++;
	}

	flush_work(&work);

	if (atomic_read(&work_counter) == 1) {
		tbridge_printf("PASS: work callback executed (count=%d)\n", atomic_read(&work_counter));
	} else {
		tbridge_printf("FAIL: work callback not executed (count=%d, expected 1)\n", atomic_read(&work_counter));
		errors++;
	}

	destroy_workqueue(wq);
	tbridge_printf("PASS: workqueue destroyed\n");

	return errors;
}

/* Test 3: Multiple work items - small batch (10 items) */
static int test_multiple_work_small(void)
{
	struct work_struct *works;
	struct workqueue_struct *wq;
	int i;
	int errors = 0;
	const int num_items = 10;

	tbridge_printf("\nTest 3: Multiple work items (%d items)...\n", num_items);

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

	tbridge_printf("INFO: Queued %d work items\n", num_items);

	drain_workqueue(wq);

	if (atomic_read(&work_counter) == num_items) {
		tbridge_printf("PASS: All %d work callbacks executed\n", atomic_read(&work_counter));
	} else {
		tbridge_printf("FAIL: Expected %d callbacks, got %d\n", num_items, atomic_read(&work_counter));
		errors++;
	}

	destroy_workqueue(wq);
	tsleep(curthread, 0, "wqdelay", hz / 4);
	kfree(works);

	return errors;
}

/* Test 4: Multiple work items - medium batch (50 items) */
static int test_multiple_work_medium(void)
{
	struct work_struct *works;
	struct workqueue_struct *wq;
	int i;
	int errors = 0;
	const int num_items = 50;

	tbridge_printf("\nTest 4: Multiple work items - medium batch (%d items)...\n", num_items);

	wq = alloc_workqueue("test_multi_med", 0, 0);
	if (wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue() failed\n");
		return 1;
	}

	atomic_set(&work_done, 0);

	works = kmalloc(sizeof(*works) * num_items, GFP_KERNEL);
	if (works == NULL) {
		tbridge_printf("FAIL: kmalloc() failed for work items\n");
		destroy_workqueue(wq);
		return 1;
	}

	for (i = 0; i < num_items; i++) {
		INIT_WORK(&works[i], test_work_count_fn);
		queue_work(wq, &works[i]);
	}

	tbridge_printf("INFO: Queued %d work items\n", num_items);

	drain_workqueue(wq);

	if (atomic_read(&work_done) == num_items) {
		tbridge_printf("PASS: All %d work callbacks executed\n", atomic_read(&work_done));
	} else {
		tbridge_printf("FAIL: Expected %d callbacks, got %d\n", num_items, atomic_read(&work_done));
		errors++;
	}

	destroy_workqueue(wq);
	tsleep(curthread, 0, "wqdelay", hz / 4);
	kfree(works);

	return errors;
}

/* Test 5: cancel_work_sync */
static int test_cancel_work(void)
{
	struct work_struct work;
	int errors = 0;

	tbridge_printf("\nTest 5: cancel_work_sync()...\n");

	atomic_set(&work_counter, 0);
	INIT_WORK(&work, test_work_fn);

	/* Queue work */
	if (queue_work(system_wq, &work)) {
		tbridge_printf("PASS: work queued\n");
		/* Cancel it immediately */
		cancel_work_sync(&work);
		tbridge_printf("PASS: cancel_work_sync() completed\n");
	} else {
		tbridge_printf("INFO: queue_work() returned false, nothing to cancel\n");
	}

	return errors;
}

/* Test 6: Stress test - rapid create/destroy cycles */
static int test_stress_create_destroy(void)
{
	struct workqueue_struct *wq;
	int i;
	int errors = 0;
	const int cycles = 20;

	tbridge_printf("\nTest 6: Stress test - %d rapid create/destroy cycles...\n", cycles);

	for (i = 0; i < cycles; i++) {
		wq = alloc_workqueue("stress_wq", 0, 4);
		if (wq == NULL) {
			tbridge_printf("FAIL: alloc_workqueue() failed at cycle %d\n", i);
			errors++;
			break;
		}
		destroy_workqueue(wq);
	}

	if (errors == 0) {
		tbridge_printf("PASS: All %d create/destroy cycles completed\n", cycles);
	}

	return errors;
}

/* Test 7: System workqueue */
static int test_system_wq(void)
{
	struct work_struct work;
	int errors = 0;

	tbridge_printf("\nTest 7: queue_work() on system_wq...\n");

	atomic_set(&work_counter, 0);
	INIT_WORK(&work, test_work_fn);

	if (queue_work(system_wq, &work)) {
		tbridge_printf("PASS: queue_work(system_wq) queued\n");
	} else {
		tbridge_printf("INFO: queue_work() returned false\n");
	}

	flush_work(&work);

	if (atomic_read(&work_counter) == 1) {
		tbridge_printf("PASS: work callback executed (count=%d)\n", atomic_read(&work_counter));
	} else {
		tbridge_printf("FAIL: work callback not executed (count=%d)\n", atomic_read(&work_counter));
		errors++;
	}

	return errors;
}

/* Main test runner */
static void
linuxkpi_workqueue_run(void *arg __unused)
{
	int total_errors = 0;

	tbridge_printf("========================================\n");
	tbridge_printf("LinuxKPI Workqueue Test Suite\n");
	tbridge_printf("Per-CPU workqueue implementation\n");
	tbridge_printf("========================================\n\n");

	total_errors += test_alloc_workqueue();
	total_errors += test_queue_work();
	total_errors += test_multiple_work_small();
	total_errors += test_multiple_work_medium();
	total_errors += test_cancel_work();
	total_errors += test_stress_create_destroy();
	total_errors += test_system_wq();

	tbridge_printf("\n========================================\n");
	if (total_errors == 0) {
		tbridge_printf("ALL WORKQUEUE TESTS PASSED!\n");
		tbridge_printf("========================================\n");
		tbridge_test_done(RESULT_PASS);
	} else {
		tbridge_printf("TESTS FAILED: %d errors\n", total_errors);
		tbridge_printf("========================================\n");
		tbridge_test_done(RESULT_FAIL);
	}
}

static struct tbridge_testcase linuxkpi_workqueue_case = {
	.tb_run		= linuxkpi_workqueue_run,
	.tb_abort	= NULL
};

TBRIDGE_TESTCASE_MODULE(linuxkpi_workqueue, &linuxkpi_workqueue_case);
