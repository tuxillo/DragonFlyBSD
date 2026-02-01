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
static atomic_t requeue_count = ATOMIC_INIT(0);
static atomic_t per_cpu_count[MAXCPU];

/* Forward declaration for requeue test */
static struct work_struct requeue_work;
static struct workqueue_struct *requeue_wq;

/* Simple work function */
static void test_work_fn(struct work_struct *work)
{
	atomic_inc(&work_counter);
}

/* Work function that re-queues itself (chained work) */
static void test_requeue_fn(struct work_struct *work)
{
	int count = atomic_inc_return(&requeue_count);
	if (count < 5) {
		/* Re-queue the work for another run */
		queue_work(requeue_wq, work);
	}
}

/* Work function - just count without delay to avoid blocking */
static void test_work_delay_fn(struct work_struct *work)
{
	atomic_inc(&work_done);
}

/* Work function that tracks which CPU it ran on */
static void test_cpu_track_fn(struct work_struct *work)
{
	int cpuid = mycpuid;
	if (cpuid >= 0 && cpuid < MAXCPU) {
		atomic_inc(&per_cpu_count[cpuid]);
	}
	atomic_inc(&work_counter);
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

/* Test 5: Multiple work items (50 items) */
static int test_multiple_work(void)
{
	struct work_struct *works;
	struct workqueue_struct *wq;
	int i;
	int errors = 0;
	const int num_items = 50;

	tbridge_printf("\nTest 5: Multiple work items (%d items)...\n", num_items);

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

	/*
	 * Use drain_workqueue() to ensure all work items complete.
	 * drain_workqueue() waits for all pending and active work.
	 */
	drain_workqueue(wq);

	if (atomic_read(&work_counter) == num_items) {
		tbridge_printf("PASS: All %d work callbacks executed\n", atomic_read(&work_counter));
	} else {
		tbridge_printf("FAIL: Expected %d callbacks, got %d\n", num_items, atomic_read(&work_counter));
		errors++;
	}

	destroy_workqueue(wq);
	
	/*
	 * Small delay to ensure any pending taskqueue cleanup completes
	 * before we free the work items. Use curthread as ident.
	 */
	tsleep(curthread, 0, "wqdelay", hz / 10);
	
	kfree(works);

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

/* Test 7: Sustained work processing with 10 items */
static int test_sustained_work(void)
{
	struct workqueue_struct *wq;
	struct work_struct *works;
	int i;
	int errors = 0;
	const int num_items = 20;

	tbridge_printf("\nTest 7: Sustained work processing (%d items)...\n", num_items);

	works = kmalloc(sizeof(struct work_struct) * num_items, GFP_KERNEL);
	if (works == NULL) {
		tbridge_printf("FAIL: kmalloc() failed\n");
		return 1;
	}

	/*
	 * Use singlethread workqueue to avoid stack overflow.
	 * Sequential execution with bounded stack usage.
	 */
	wq = create_singlethread_workqueue("test_sustained");
	if (wq == NULL) {
		tbridge_printf("FAIL: create_singlethread_workqueue() failed\n");
		kfree(works);
		return 1;
	}

	atomic_set(&work_done, 0);

	for (i = 0; i < num_items; i++) {
		INIT_WORK(&works[i], test_work_delay_fn);
		queue_work(wq, &works[i]);
	}

	tbridge_printf("INFO: Queued %d work items\n", num_items);

	/*
	 * Use drain_workqueue() for sustained work - this ensures ALL work
	 * items complete, including those still queued.
	 */
	drain_workqueue(wq);

	if (atomic_read(&work_done) == num_items) {
		tbridge_printf("PASS: All %d work callbacks executed\n", atomic_read(&work_done));
	} else {
		tbridge_printf("FAIL: Expected %d callbacks, got %d\n", num_items, atomic_read(&work_done));
		errors++;
	}

	/* No need to cancel - drain_workqueue ensures all work completed */
	tbridge_printf("INFO: All work items completed\n");
	
	tbridge_printf("INFO: About to call destroy_workqueue()...\n");
	destroy_workqueue(wq);
	tbridge_printf("INFO: destroy_workqueue() returned\n");
	
	/*
	 * Small delay to ensure any pending taskqueue cleanup completes
	 * before we free the work items. Use curthread as ident.
	 */
	tsleep(curthread, 0, "wqdelay", hz / 10);
	
	kfree(works);
	tbridge_printf("INFO: kfree() returned, test_sustained_work done\n");

	return errors;
}

/* Test 8: Work re-queueing (work that queues more work) */
static int test_requeue_work(void)
{
	int errors = 0;

	tbridge_printf("\nTest 8: Work re-queueing (chained work)...\n");

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

/* Test 9: Multiple concurrent workqueues */
static int test_concurrent_workqueues(void)
{
	struct workqueue_struct *wq1, *wq2, *wq3;
	struct work_struct work1, work2, work3;
	int errors = 0;

	tbridge_printf("\nTest 9: Multiple concurrent workqueues...\n");

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

/* Test 10: Per-CPU distribution verification */
static int test_per_cpu_distribution(void)
{
	struct work_struct *works;
	struct workqueue_struct *wq;
	int i;
	int errors = 0;
	int num_cpus = ncpus;
	int work_items = num_cpus * 5;  /* 5 items per CPU */

	tbridge_printf("\nTest 10: Per-CPU distribution (%d CPUs, %d items)...\n", num_cpus, work_items);

	/* Initialize per-CPU counters (only up to ncpus, not MAXCPU) */
	for (i = 0; i < num_cpus && i < MAXCPU; i++) {
		atomic_set(&per_cpu_count[i], 0);
	}
	atomic_set(&work_counter, 0);

	wq = alloc_workqueue("test_percpu", 0, 0);  /* Per-CPU workqueue */
	if (wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue() failed\n");
		return 1;
	}

	works = kmalloc(sizeof(*works) * work_items, GFP_KERNEL);
	if (works == NULL) {
		tbridge_printf("FAIL: kmalloc() failed for work items\n");
		destroy_workqueue(wq);
		return 1;
	}

	for (i = 0; i < work_items; i++) {
		INIT_WORK(&works[i], test_cpu_track_fn);
		queue_work(wq, &works[i]);
	}

	tbridge_printf("INFO: Queued %d work items\n", work_items);

	drain_workqueue(wq);

	if (atomic_read(&work_counter) == work_items) {
		tbridge_printf("PASS: All %d work callbacks executed\n", work_items);
		
		/* Show distribution across CPUs */
		tbridge_printf("INFO: Per-CPU execution counts:\n");
		for (i = 0; i < num_cpus && i < MAXCPU; i++) {
			int count = atomic_read(&per_cpu_count[i]);
			if (count > 0) {
				tbridge_printf("  CPU %d: %d items\n", i, count);
			}
		}
	} else {
		tbridge_printf("FAIL: Expected %d callbacks, got %d\n", 
			work_items, atomic_read(&work_counter));
		errors++;
	}

	destroy_workqueue(wq);
	tsleep(curthread, 0, "wqdelay", hz / 10);
	kfree(works);

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
	total_errors += test_requeue_work();
	total_errors += test_concurrent_workqueues();
	total_errors += test_per_cpu_distribution();

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
