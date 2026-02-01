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

/* Work function - just count without delay to avoid blocking */
static void test_work_delay_fn(struct work_struct *work)
{
	atomic_inc(&work_done);
}

/* Test 1: alloc_workqueue and destroy_workqueue */
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
		tbridge_printf("PASS: alloc_workqueue() created workqueue\n");
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
	}

	return errors;
}

/* Test 2: queue_work on custom workqueue */
static int test_queue_work(void)
{
	struct workqueue_struct *wq;
	struct work_struct work;
	int errors = 0;

	tbridge_printf("\nTest 2: queue_work() on custom workqueue...\n");

	wq = alloc_workqueue("test_wq", 0, 1);
	if (wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue() failed\n");
		return 1;
	}

	atomic_set(&work_counter, 0);
	INIT_WORK(&work, test_work_fn);

	if (queue_work(wq, &work)) {
		tbridge_printf("PASS: queue_work() queued\n");
	} else {
		tbridge_printf("INFO: queue_work() returned false (already pending)\n");
	}

	flush_work(&work);

	if (atomic_read(&work_counter) == 1) {
		tbridge_printf("PASS: work callback executed (count=%d)\n", atomic_read(&work_counter));
	} else {
		tbridge_printf("FAIL: work callback not executed (count=%d)\n", atomic_read(&work_counter));
		errors++;
	}

	destroy_workqueue(wq);
	tbridge_printf("PASS: workqueue destroyed\n");

	return errors;
}

/* Test 3: queue_work on system_wq */
static int test_system_wq(void)
{
	struct work_struct work;
	int errors = 0;

	tbridge_printf("\nTest 3: queue_work() on system_wq...\n");

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

/* Test 4: schedule_work (convenience function) */
static int test_schedule_work(void)
{
	struct work_struct work;
	int errors = 0;

	tbridge_printf("\nTest 4: schedule_work() convenience function...\n");

	atomic_set(&work_counter, 0);
	INIT_WORK(&work, test_work_fn);

	if (schedule_work(&work)) {
		tbridge_printf("PASS: schedule_work() queued\n");
	} else {
		tbridge_printf("INFO: schedule_work() returned false\n");
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

/* Test 5: Multiple work items */
static int test_multiple_work(void)
{
	struct work_struct works[5];
	struct workqueue_struct *wq;
	int i;
	int errors = 0;

	tbridge_printf("\nTest 5: Multiple work items...\n");

	wq = alloc_workqueue("test_multi", 0, 4);
	if (wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue() failed\n");
		return 1;
	}

	atomic_set(&work_counter, 0);

	for (i = 0; i < 5; i++) {
		INIT_WORK(&works[i], test_work_fn);
		queue_work(wq, &works[i]);
	}

	tbridge_printf("INFO: Queued 5 work items\n");

	/* Flush all work */
	flush_workqueue(wq);

	if (atomic_read(&work_counter) == 5) {
		tbridge_printf("PASS: All %d work callbacks executed\n", atomic_read(&work_counter));
	} else {
		tbridge_printf("FAIL: Expected 5 callbacks, got %d\n", atomic_read(&work_counter));
		errors++;
	}

	destroy_workqueue(wq);

	return errors;
}

/* Test 6: cancel_work_sync */
static int test_cancel_work(void)
{
	struct work_struct work;
	int errors = 0;

	tbridge_printf("\nTest 6: cancel_work_sync()...\n");

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

/* Test 7: Sustained work processing with 100 items */
static int test_sustained_work(void)
{
	struct workqueue_struct *wq;
	struct work_struct *works;
	int i;
	int errors = 0;

	tbridge_printf("\nTest 7: Sustained work processing (100 items)...\n");

	works = kmalloc(sizeof(struct work_struct) * 100, GFP_KERNEL);
	if (works == NULL) {
		tbridge_printf("FAIL: kmalloc() failed\n");
		return 1;
	}

	wq = alloc_workqueue("test_sustained", 0, 4);
	if (wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue() failed\n");
		kfree(works);
		return 1;
	}

	atomic_set(&work_done, 0);

	for (i = 0; i < 100; i++) {
		INIT_WORK(&works[i], test_work_delay_fn);
		queue_work(wq, &works[i]);
	}

	tbridge_printf("INFO: Queued 100 work items\n");

	/* Flush to ensure all complete */
	flush_workqueue(wq);

	if (atomic_read(&work_done) == 100) {
		tbridge_printf("PASS: All %d work callbacks executed\n", atomic_read(&work_done));
	} else {
		tbridge_printf("FAIL: Expected 100 callbacks, got %d\n", atomic_read(&work_done));
		errors++;
	}

	/* Cancel any remaining work items to ensure they're detached from taskqueue */
	for (i = 0; i < 100; i++) {
		cancel_work(&works[i]);
	}
	tbridge_printf("INFO: Cancelled all work items\n");
	
	tbridge_printf("INFO: About to call destroy_workqueue()...\n");
	destroy_workqueue(wq);
	tbridge_printf("INFO: destroy_workqueue() returned\n");
	kfree(works);
	tbridge_printf("INFO: kfree() returned, test_sustained_work done\n");

	return errors;
}

/* Main test runner */
static void
linuxkpi_workqueue_run(void *arg __unused)
{
	int total_errors = 0;

	tbridge_printf("========================================\n");
	tbridge_printf("LinuxKPI Workqueue Functional Test\n");
	tbridge_printf("Actually exercising workqueue APIs!\n");
	tbridge_printf("========================================\n\n");

	total_errors += test_alloc_workqueue();
	total_errors += test_queue_work();
	total_errors += test_system_wq();
	total_errors += test_schedule_work();
	total_errors += test_multiple_work();
	total_errors += test_cancel_work();
	total_errors += test_sustained_work();

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
