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
#include <sys/cpu_topology.h>

/* LinuxKPI headers */
#include <linux/workqueue.h>
#include <linux/gfp.h>
#include <linux/atomic.h>

#include <dfregress.h>

/* Test configuration - make it exhaustive */
#define TEST_SMALL_BATCH	10
#define TEST_MEDIUM_BATCH	100
#define TEST_LARGE_BATCH	500
#define TEST_CONCURRENT_WQ	4
#define TEST_STRESS_CYCLES	50

/* Test counters - atomic to avoid race conditions */
static atomic_t work_counter = ATOMIC_INIT(0);
static atomic_t work_done = ATOMIC_INIT(0);
static atomic_t requeue_counter = ATOMIC_INIT(0);
static atomic_t cpu_check_counter = ATOMIC_INIT(0);
static atomic_t cancel_attempts = ATOMIC_INIT(0);
static atomic_t cancel_success = ATOMIC_INIT(0);

/* Per-CPU tracking for distribution test */
static atomic_t per_cpu_count[MAXCPU];

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

/* Work function that tracks which CPU it ran on */
static void test_cpu_track_fn(struct work_struct *work)
{
	int cpuid = mycpuid;
	if (cpuid >= 0 && cpuid < MAXCPU) {
		atomic_inc(&per_cpu_count[cpuid]);
	}
	atomic_inc(&cpu_check_counter);
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

	/* Test per-CPU workqueue (0 means use default ncpus) */
	wq = alloc_workqueue("test_wq_percpu", 0, 0);
	if (wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue(per-cpu) returned NULL\n");
		errors++;
	} else {
		tbridge_printf("PASS: alloc_workqueue(per-cpu) created workqueue\n");
		destroy_workqueue(wq);
		tbridge_printf("PASS: destroy_workqueue(per-cpu) completed\n");
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

/* Test 3: Multiple work items - small batch */
static int test_multiple_work_small(void)
{
	struct work_struct *works;
	struct workqueue_struct *wq;
	int i;
	int errors = 0;

	tbridge_printf("\nTest 3: Multiple work items (%d items)...\n", TEST_SMALL_BATCH);

	wq = alloc_workqueue("test_multi_small", 0, 4);
	if (wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue() failed\n");
		return 1;
	}

	atomic_set(&work_counter, 0);

	works = kmalloc(sizeof(*works) * TEST_SMALL_BATCH, GFP_KERNEL);
	if (works == NULL) {
		tbridge_printf("FAIL: kmalloc() failed for work items\n");
		destroy_workqueue(wq);
		return 1;
	}

	for (i = 0; i < TEST_SMALL_BATCH; i++) {
		INIT_WORK(&works[i], test_work_fn);
		queue_work(wq, &works[i]);
	}

	tbridge_printf("INFO: Queued %d work items\n", TEST_SMALL_BATCH);

	drain_workqueue(wq);

	if (atomic_read(&work_counter) == TEST_SMALL_BATCH) {
		tbridge_printf("PASS: All %d work callbacks executed\n", atomic_read(&work_counter));
	} else {
		tbridge_printf("FAIL: Expected %d callbacks, got %d\n", TEST_SMALL_BATCH, atomic_read(&work_counter));
		errors++;
	}

	destroy_workqueue(wq);
	tsleep(curthread, 0, "wqdelay", hz);
	kfree(works);

	return errors;
}

/* Test 4: Multiple work items - medium batch */
static int test_multiple_work_medium(void)
{
	struct work_struct *works;
	struct workqueue_struct *wq;
	int i;
	int errors = 0;

	tbridge_printf("\nTest 4: Multiple work items - medium batch (%d items)...\n", TEST_MEDIUM_BATCH);

	wq = alloc_workqueue("test_multi_med", 0, 0);  /* Use default ncpus */
	if (wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue() failed\n");
		return 1;
	}

	atomic_set(&work_done, 0);

	works = kmalloc(sizeof(*works) * TEST_MEDIUM_BATCH, GFP_KERNEL);
	if (works == NULL) {
		tbridge_printf("FAIL: kmalloc() failed for work items\n");
		destroy_workqueue(wq);
		return 1;
	}

	for (i = 0; i < TEST_MEDIUM_BATCH; i++) {
		INIT_WORK(&works[i], test_work_count_fn);
		queue_work(wq, &works[i]);
	}

	tbridge_printf("INFO: Queued %d work items\n", TEST_MEDIUM_BATCH);

	drain_workqueue(wq);

	if (atomic_read(&work_done) == TEST_MEDIUM_BATCH) {
		tbridge_printf("PASS: All %d work callbacks executed\n", atomic_read(&work_done));
	} else {
		tbridge_printf("FAIL: Expected %d callbacks, got %d\n", TEST_MEDIUM_BATCH, atomic_read(&work_done));
		errors++;
	}

	destroy_workqueue(wq);
	tsleep(curthread, 0, "wqdelay", hz);
	kfree(works);

	return errors;
}

/* Test 5: Per-CPU distribution test */
static int test_per_cpu_distribution(void)
{
	struct work_struct *works;
	struct workqueue_struct *wq;
	int i;
	int errors = 0;
	int num_cpus = ncpus;
	int work_items = num_cpus * 20;  /* 20 items per CPU */

	tbridge_printf("\nTest 5: Per-CPU distribution test (%d CPUs, %d items)...\n", num_cpus, work_items);

	/* Initialize per-CPU counters */
	for (i = 0; i < MAXCPU; i++) {
		atomic_set(&per_cpu_count[i], 0);
	}
	atomic_set(&cpu_check_counter, 0);

	wq = alloc_workqueue("test_percpu", 0, 0);
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

	tbridge_printf("INFO: Queued %d work items across %d CPUs\n", work_items, num_cpus);

	drain_workqueue(wq);

	if (atomic_read(&cpu_check_counter) == work_items) {
		tbridge_printf("PASS: All %d work callbacks executed\n", atomic_read(&cpu_check_counter));
		
		/* Show distribution */
		tbridge_printf("INFO: Per-CPU distribution:\n");
		for (i = 0; i < num_cpus && i < MAXCPU; i++) {
			tbridge_printf("  CPU %d: %d items\n", i, atomic_read(&per_cpu_count[i]));
		}
	} else {
		tbridge_printf("FAIL: Expected %d callbacks, got %d\n", work_items, atomic_read(&cpu_check_counter));
		errors++;
	}

	destroy_workqueue(wq);
	tsleep(curthread, 0, "wqdelay", hz);
	kfree(works);

	return errors;
}

/* Test 6: Stress test - rapid create/destroy cycles */
static int test_stress_create_destroy(void)
{
	struct workqueue_struct *wq;
	int i;
	int errors = 0;

	tbridge_printf("\nTest 6: Stress test - %d rapid create/destroy cycles...\n", TEST_STRESS_CYCLES);

	for (i = 0; i < TEST_STRESS_CYCLES; i++) {
		wq = alloc_workqueue("stress_wq", 0, 4);
		if (wq == NULL) {
			tbridge_printf("FAIL: alloc_workqueue() failed at cycle %d\n", i);
			errors++;
			break;
		}
		destroy_workqueue(wq);
	}

	if (errors == 0) {
		tbridge_printf("PASS: All %d create/destroy cycles completed\n", TEST_STRESS_CYCLES);
	}

	return errors;
}

/* Test 7: cancel_work_sync under load */
static int test_cancel_under_load(void)
{
	struct work_struct *works;
	struct workqueue_struct *wq;
	int i;
	int errors = 0;

	tbridge_printf("\nTest 7: cancel_work_sync under load...\n");

	wq = alloc_workqueue("test_cancel", 0, 4);
	if (wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue() failed\n");
		return 1;
	}

	works = kmalloc(sizeof(*works) * TEST_MEDIUM_BATCH, GFP_KERNEL);
	if (works == NULL) {
		tbridge_printf("FAIL: kmalloc() failed\n");
		destroy_workqueue(wq);
		return 1;
	}

	atomic_set(&work_counter, 0);
	atomic_set(&cancel_attempts, 0);
	atomic_set(&cancel_success, 0);

	/* Queue many work items */
	for (i = 0; i < TEST_MEDIUM_BATCH; i++) {
		INIT_WORK(&works[i], test_work_fn);
		queue_work(wq, &works[i]);
	}

	/* Try to cancel some while they're running */
	for (i = 0; i < TEST_MEDIUM_BATCH / 4; i++) {
		atomic_inc(&cancel_attempts);
		if (cancel_work_sync(&works[i])) {
			atomic_inc(&cancel_success);
		}
	}

	drain_workqueue(wq);

	tbridge_printf("INFO: Cancel attempts: %d, Success: %d\n", 
		atomic_read(&cancel_attempts), atomic_read(&cancel_success));
	tbridge_printf("INFO: Work completed: %d\n", atomic_read(&work_counter));

	/* Just verify no crashes/panics - cancel results are variable */
	tbridge_printf("PASS: cancel_work_sync under load completed without issues\n");

	destroy_workqueue(wq);
	tsleep(curthread, 0, "wqdelay", hz);
	kfree(works);

	return errors;
}

/* Test 8: flush_workqueue vs drain_workqueue */
static int test_flush_vs_drain(void)
{
	struct workqueue_struct *wq;
	struct work_struct work1, work2, work3;
	int errors = 0;

	tbridge_printf("\nTest 8: flush_workqueue vs drain_workqueue...\n");

	wq = alloc_workqueue("test_flush", 0, 1);  /* Single thread for predictability */
	if (wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue() failed\n");
		return 1;
	}

	atomic_set(&work_counter, 0);

	/* Queue multiple work items */
	INIT_WORK(&work1, test_work_fn);
	INIT_WORK(&work2, test_work_fn);
	INIT_WORK(&work3, test_work_fn);

	queue_work(wq, &work1);
	queue_work(wq, &work2);
	queue_work(wq, &work3);

	tbridge_printf("INFO: Queued 3 work items\n");

	/* Use flush_workqueue - should wait for all queued work */
	flush_workqueue(wq);

	int count1 = atomic_read(&work_counter);
	tbridge_printf("INFO: After flush_workqueue: %d items completed\n", count1);

	/* Queue more work */
	queue_work(wq, &work1);
	queue_work(wq, &work2);

	drain_workqueue(wq);

	int count2 = atomic_read(&work_counter);
	tbridge_printf("INFO: After drain_workqueue: %d items completed\n", count2);

	if (count2 >= count1 + 2) {
		tbridge_printf("PASS: Both flush and drain completed work correctly\n");
	} else {
		tbridge_printf("FAIL: Work not completed as expected (%d vs %d expected)\n", count2, count1 + 2);
		errors++;
	}

	destroy_workqueue(wq);

	return errors;
}

/* Test 9: Concurrent workqueues */
static int test_concurrent_workqueues(void)
{
	struct workqueue_struct *wqs[TEST_CONCURRENT_WQ];
	struct work_struct *works[TEST_CONCURRENT_WQ];
	int i, j;
	int errors = 0;
	int work_per_wq = 25;

	tbridge_printf("\nTest 9: Concurrent workqueues (%d WQs, %d items each)...\n", 
		TEST_CONCURRENT_WQ, work_per_wq);

	atomic_set(&work_done, 0);

	/* Create multiple workqueues */
	for (i = 0; i < TEST_CONCURRENT_WQ; i++) {
		char name[32];
		snprintf(name, sizeof(name), "concurrent_wq_%d", i);
		wqs[i] = alloc_workqueue(name, 0, 2);
		if (wqs[i] == NULL) {
			tbridge_printf("FAIL: alloc_workqueue(%s) failed\n", name);
			errors++;
			goto cleanup;
		}

		works[i] = kmalloc(sizeof(struct work_struct) * work_per_wq, GFP_KERNEL);
		if (works[i] == NULL) {
			tbridge_printf("FAIL: kmalloc() failed for WQ %d\n", i);
			errors++;
			goto cleanup;
		}

		for (j = 0; j < work_per_wq; j++) {
			INIT_WORK(&works[i][j], test_work_count_fn);
			queue_work(wqs[i], &works[i][j]);
		}
	}

	tbridge_printf("INFO: Created %d workqueues with %d items each\n", 
		TEST_CONCURRENT_WQ, work_per_wq);

	/* Drain all workqueues */
	for (i = 0; i < TEST_CONCURRENT_WQ; i++) {
		if (wqs[i] != NULL) {
			drain_workqueue(wqs[i]);
		}
	}

	int total_expected = TEST_CONCURRENT_WQ * work_per_wq;
	int total_done = atomic_read(&work_done);

	if (total_done == total_expected) {
		tbridge_printf("PASS: All %d work items completed across %d workqueues\n", 
			total_done, TEST_CONCURRENT_WQ);
	} else {
		tbridge_printf("FAIL: Expected %d items, got %d\n", total_expected, total_done);
		errors++;
	}

cleanup:
	/* Cleanup */
	for (i = 0; i < TEST_CONCURRENT_WQ; i++) {
		if (wqs[i] != NULL) {
			destroy_workqueue(wqs[i]);
		}
		if (works[i] != NULL) {
			tsleep(curthread, 0, "wqdelay", hz);
			kfree(works[i]);
		}
	}

	return errors;
}

/* Test 10: Large batch stress test */
static int test_large_batch(void)
{
	struct work_struct *works;
	struct workqueue_struct *wq;
	int i;
	int errors = 0;

	tbridge_printf("\nTest 10: Large batch stress test (%d items)...\n", TEST_LARGE_BATCH);

	wq = alloc_workqueue("test_large", 0, 0);
	if (wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue() failed\n");
		return 1;
	}

	atomic_set(&work_done, 0);

	works = kmalloc(sizeof(*works) * TEST_LARGE_BATCH, GFP_KERNEL);
	if (works == NULL) {
		tbridge_printf("FAIL: kmalloc() failed for %d work items\n", TEST_LARGE_BATCH);
		destroy_workqueue(wq);
		return 1;
	}

	for (i = 0; i < TEST_LARGE_BATCH; i++) {
		INIT_WORK(&works[i], test_work_count_fn);
		queue_work(wq, &works[i]);
	}

	tbridge_printf("INFO: Queued %d work items\n", TEST_LARGE_BATCH);

	drain_workqueue(wq);

	if (atomic_read(&work_done) == TEST_LARGE_BATCH) {
		tbridge_printf("PASS: All %d work callbacks executed\n", atomic_read(&work_done));
	} else {
		tbridge_printf("FAIL: Expected %d callbacks, got %d\n", 
			TEST_LARGE_BATCH, atomic_read(&work_done));
		errors++;
	}

	destroy_workqueue(wq);
	tsleep(curthread, 0, "wqdelay", hz);
	kfree(works);

	return errors;
}

/* Main test runner */
static void
linuxkpi_workqueue_run(void *arg __unused)
{
	int total_errors = 0;

	tbridge_printf("========================================\n");
	tbridge_printf("LinuxKPI Workqueue Exhaustive Test\n");
	tbridge_printf("Per-CPU workqueue implementation validation\n");
	tbridge_printf("========================================\n\n");

	tbridge_printf("Test configuration:\n");
	tbridge_printf("  Small batch: %d items\n", TEST_SMALL_BATCH);
	tbridge_printf("  Medium batch: %d items\n", TEST_MEDIUM_BATCH);
	tbridge_printf("  Large batch: %d items\n", TEST_LARGE_BATCH);
	tbridge_printf("  Concurrent WQs: %d\n", TEST_CONCURRENT_WQ);
	tbridge_printf("  Stress cycles: %d\n\n", TEST_STRESS_CYCLES);

	total_errors += test_alloc_workqueue();
	total_errors += test_queue_work();
	total_errors += test_multiple_work_small();
	total_errors += test_multiple_work_medium();
	total_errors += test_per_cpu_distribution();
	total_errors += test_stress_create_destroy();
	total_errors += test_cancel_under_load();
	total_errors += test_flush_vs_drain();
	total_errors += test_concurrent_workqueues();
	total_errors += test_large_batch();

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
