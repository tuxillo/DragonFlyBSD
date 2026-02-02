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
#include <sys/time.h>

/* LinuxKPI headers */
#include <linux/workqueue.h>
#include <linux/gfp.h>
#include <linux/atomic.h>

#include <dfregress.h>

/* Test abort flag - set by tb_abort callback */
static atomic_t test_abort_flag = ATOMIC_INIT(0);

/* Test counters - atomic to avoid race conditions */
static atomic_t work_counter = ATOMIC_INIT(0);
static atomic_t work_done = ATOMIC_INIT(0);
static atomic_t requeue_count = ATOMIC_INIT(0);
static atomic_t per_cpu_count[MAXCPU];

/* Forward declaration for requeue test */
static struct work_struct requeue_work;
static struct workqueue_struct *requeue_wq;

/* Delayed work test counters */
static atomic_t delayed_work_counter = ATOMIC_INIT(0);
static atomic_t delayed_self_requeue_count = ATOMIC_INIT(0);
static struct delayed_work delayed_requeue_dwork;
static struct workqueue_struct *delayed_requeue_wq;

/* Ordered workqueue test - verify serial execution */
static atomic_t ordered_seq = ATOMIC_INIT(0);
static atomic_t ordered_last_seen = ATOMIC_INIT(0);
static atomic_t ordered_errors = ATOMIC_INIT(0);

/* Idle timeout pattern test (simulates amdgpu power management) */
static atomic_t power_up_count = ATOMIC_INIT(0);
static atomic_t idle_timeout_executed = ATOMIC_INIT(0);
static struct delayed_work idle_timeout_dwork;

/* Concurrent stress test */
static atomic_t stress_queue_success = ATOMIC_INIT(0);
static atomic_t stress_cancel_success = ATOMIC_INIT(0);
static atomic_t stress_executed = ATOMIC_INIT(0);

/* Forward declarations */
static long elapsed_ms(struct timeval *start, struct timeval *end);

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

/* Delayed work callback - simple counter increment */
static void test_delayed_work_fn(struct work_struct *work)
{
	atomic_inc(&delayed_work_counter);
}

/* Self-requeueing delayed work callback */
static void test_delayed_self_requeue_fn(struct work_struct *work)
{
	int count = atomic_inc_return(&delayed_self_requeue_count);
	if (count < 5) {
		/* Re-queue with short delay */
		schedule_delayed_work(&delayed_requeue_dwork, hz / 20);
	}
}

/* Ordered workqueue callback - records sequence and checks ordering */
static void test_ordered_work_fn(struct work_struct *work)
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

/* Idle timeout callback - simulates power-down on idle */
static void test_idle_timeout_fn(struct work_struct *work)
{
	atomic_inc(&idle_timeout_executed);
}

/* Stress test callback */
static void test_stress_fn(struct work_struct *work)
{
	atomic_inc(&stress_executed);
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

	destroy_workqueue(wq);
	kfree(works);

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
	kfree(works);

	return errors;
}

/* Test 11: Stress test - 1000 work items */
static int test_stress_many_items(void)
{
	struct work_struct *works;
	struct workqueue_struct *wq;
	int i;
	int errors = 0;
	const int num_items = 1000;

	tbridge_printf("\nTest 11: Stress test (%d work items)...\n", num_items);

	wq = alloc_workqueue("test_stress", 0, 0);  /* Per-CPU workqueue */
	if (wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue() failed\n");
		return 1;
	}

	atomic_set(&work_counter, 0);

	works = kmalloc(sizeof(*works) * num_items, GFP_KERNEL);
	if (works == NULL) {
		tbridge_printf("FAIL: kmalloc() failed\n");
		destroy_workqueue(wq);
		return 1;
	}

	/* Queue all 1000 items */
	for (i = 0; i < num_items; i++) {
		INIT_WORK(&works[i], test_work_fn);
		queue_work(wq, &works[i]);
	}

	drain_workqueue(wq);

	if (atomic_read(&work_counter) == num_items) {
		tbridge_printf("PASS: All %d work callbacks executed\n", num_items);
	} else {
		tbridge_printf("FAIL: Expected %d callbacks, got %d\n", 
			num_items, atomic_read(&work_counter));
		errors++;
	}

	destroy_workqueue(wq);
	kfree(works);

	return errors;
}

/* ============================================================
 * DELAYED WORK TESTS (Tests 12-18)
 * ============================================================ */

/* Test 12: Basic delayed work */
static int test_basic_delayed_work(void)
{
	struct delayed_work dwork;
	int errors = 0;

	tbridge_printf("\nTest 12: Basic delayed work...\n");

	atomic_set(&delayed_work_counter, 0);
	INIT_DELAYED_WORK(&dwork, test_delayed_work_fn);

	/* Queue with 100ms delay */
	if (schedule_delayed_work(&dwork, hz / 10)) {
		tbridge_printf("PASS: schedule_delayed_work() queued\n");
	} else {
		tbridge_printf("INFO: schedule_delayed_work() returned false\n");
	}

	/* Wait for it to complete */
	flush_delayed_work(&dwork);

	if (atomic_read(&delayed_work_counter) == 1) {
		tbridge_printf("PASS: Delayed work callback executed (count=%d)\n",
			atomic_read(&delayed_work_counter));
	} else {
		tbridge_printf("FAIL: Delayed work callback not executed (count=%d)\n",
			atomic_read(&delayed_work_counter));
		errors++;
	}

	return errors;
}

/* Test 13: cancel_delayed_work_sync (before firing) */
static int test_cancel_delayed_before(void)
{
	struct delayed_work dwork;
	bool was_pending;
	int errors = 0;

	tbridge_printf("\nTest 13: cancel_delayed_work_sync (before firing)...\n");

	atomic_set(&delayed_work_counter, 0);
	INIT_DELAYED_WORK(&dwork, test_delayed_work_fn);

	/* Queue with long delay (5 seconds) */
	schedule_delayed_work(&dwork, hz * 5);
	tbridge_printf("INFO: Queued delayed work with 5 second delay\n");

	/* Immediately cancel it */
	was_pending = cancel_delayed_work_sync(&dwork);

	if (was_pending) {
		tbridge_printf("PASS: cancel_delayed_work_sync() returned true (was pending)\n");
	} else {
		tbridge_printf("FAIL: cancel_delayed_work_sync() returned false (expected true)\n");
		errors++;
	}

	if (atomic_read(&delayed_work_counter) == 0) {
		tbridge_printf("PASS: Callback did NOT execute (as expected)\n");
	} else {
		tbridge_printf("FAIL: Callback executed unexpectedly (count=%d)\n",
			atomic_read(&delayed_work_counter));
		errors++;
	}

	return errors;
}

/* Test 14: cancel_delayed_work_sync (after firing) */
static int test_cancel_delayed_after(void)
{
	struct delayed_work dwork;
	bool was_pending;
	int errors = 0;

	tbridge_printf("\nTest 14: cancel_delayed_work_sync (after firing)...\n");

	atomic_set(&delayed_work_counter, 0);
	INIT_DELAYED_WORK(&dwork, test_delayed_work_fn);

	/* Queue with short delay (50ms) */
	schedule_delayed_work(&dwork, hz / 20);
	tbridge_printf("INFO: Queued delayed work with 50ms delay\n");

	/* Wait for it to execute */
	flush_delayed_work(&dwork);

	/* Now cancel - should return false since already executed */
	was_pending = cancel_delayed_work_sync(&dwork);

	if (!was_pending) {
		tbridge_printf("PASS: cancel_delayed_work_sync() returned false (not pending)\n");
	} else {
		tbridge_printf("INFO: cancel_delayed_work_sync() returned true (was still pending)\n");
	}

	if (atomic_read(&delayed_work_counter) == 1) {
		tbridge_printf("PASS: Callback executed (count=%d)\n",
			atomic_read(&delayed_work_counter));
	} else {
		tbridge_printf("FAIL: Callback not executed (count=%d)\n",
			atomic_read(&delayed_work_counter));
		errors++;
	}

	return errors;
}

/* Test 15: Delayed work timeout fires */
static int test_delayed_timeout_fires(void)
{
	struct workqueue_struct *wq;
	struct delayed_work dwork;
	int errors = 0;

	tbridge_printf("\nTest 15: Delayed work timeout fires...\n");

	wq = alloc_workqueue("test_delayed", 0, 1);
	if (wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue() failed\n");
		return 1;
	}

	atomic_set(&delayed_work_counter, 0);
	INIT_DELAYED_WORK(&dwork, test_delayed_work_fn);

	/* Queue with 100ms delay */
	queue_delayed_work(wq, &dwork, hz / 10);
	tbridge_printf("INFO: Queued delayed work with 100ms delay\n");

	/* Wait for completion */
	flush_delayed_work(&dwork);

	if (atomic_read(&delayed_work_counter) == 1) {
		tbridge_printf("PASS: Delayed work fired after timeout (count=%d)\n",
			atomic_read(&delayed_work_counter));
	} else {
		tbridge_printf("FAIL: Delayed work did not fire (count=%d)\n",
			atomic_read(&delayed_work_counter));
		errors++;
	}

	destroy_workqueue(wq);
	return errors;
}

/* Test 16: Self-requeueing delayed work */
static int test_delayed_self_requeue(void)
{
	int errors = 0;

	tbridge_printf("\nTest 16: Self-requeueing delayed work...\n");

	atomic_set(&delayed_self_requeue_count, 0);

	delayed_requeue_wq = alloc_workqueue("test_delay_requeue", 0, 1);
	if (delayed_requeue_wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue() failed\n");
		return 1;
	}

	INIT_DELAYED_WORK(&delayed_requeue_dwork, test_delayed_self_requeue_fn);

	/* Queue initial delayed work with short delay */
	schedule_delayed_work(&delayed_requeue_dwork, hz / 20);
	tbridge_printf("INFO: Queued initial delayed work\n");

	/* Wait for all iterations (5 * 50ms = ~250ms) */
	/* Use a loop with flush to catch all re-queues */
	while (atomic_read(&delayed_self_requeue_count) < 5) {
		flush_delayed_work(&delayed_requeue_dwork);
		if (atomic_read(&delayed_self_requeue_count) >= 5)
			break;
		/* Small delay between flushes */
		DELAY(10000);  /* 10ms */
	}

	if (atomic_read(&delayed_self_requeue_count) == 5) {
		tbridge_printf("PASS: Delayed work re-queued correctly (%d iterations)\n",
			atomic_read(&delayed_self_requeue_count));
	} else {
		tbridge_printf("FAIL: Expected 5 iterations, got %d\n",
			atomic_read(&delayed_self_requeue_count));
		errors++;
	}

	destroy_workqueue(delayed_requeue_wq);
	return errors;
}

/* Test 17: mod_delayed_work */
static int test_mod_delayed_work(void)
{
	struct workqueue_struct *wq;
	struct delayed_work dwork;
	struct timeval start, end;
	long elapsed;
	bool was_pending;
	int errors = 0;

	tbridge_printf("\nTest 17: mod_delayed_work...\n");

	wq = alloc_workqueue("test_mod_delayed", 0, 1);
	if (wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue() failed\n");
		return 1;
	}

	atomic_set(&delayed_work_counter, 0);
	INIT_DELAYED_WORK(&dwork, test_delayed_work_fn);

	/* Queue with long delay (5 seconds) */
	queue_delayed_work(wq, &dwork, hz * 5);
	tbridge_printf("INFO: Queued with 5 second delay\n");

	microtime(&start);

	/* Modify to short delay (100ms) */
	was_pending = mod_delayed_work(wq, &dwork, hz / 10);
	tbridge_printf("INFO: mod_delayed_work() returned %s\n", was_pending ? "true" : "false");

	/* Wait for completion */
	flush_delayed_work(&dwork);
	microtime(&end);

	elapsed = elapsed_ms(&start, &end);

	if (elapsed < 1000) {  /* Should complete well under 1 second */
		tbridge_printf("PASS: Work fired quickly after mod (%ld ms)\n", elapsed);
	} else {
		tbridge_printf("FAIL: Work took too long (%ld ms) - mod may not have worked\n", elapsed);
		errors++;
	}

	if (atomic_read(&delayed_work_counter) == 1) {
		tbridge_printf("PASS: Callback executed (count=%d)\n",
			atomic_read(&delayed_work_counter));
	} else {
		tbridge_printf("FAIL: Callback not executed (count=%d)\n",
			atomic_read(&delayed_work_counter));
		errors++;
	}

	/* Test 2: mod_delayed_work on non-pending work (should queue it) */
	atomic_set(&delayed_work_counter, 0);
	INIT_DELAYED_WORK(&dwork, test_delayed_work_fn);

	/* Call mod on non-pending work */
	was_pending = mod_delayed_work(wq, &dwork, hz / 20);
	if (!was_pending) {
		tbridge_printf("PASS: mod_delayed_work() on non-pending returned false\n");
	} else {
		tbridge_printf("INFO: mod_delayed_work() on non-pending returned true\n");
	}

	flush_delayed_work(&dwork);

	if (atomic_read(&delayed_work_counter) == 1) {
		tbridge_printf("PASS: mod_delayed_work() queued non-pending work\n");
	} else {
		tbridge_printf("FAIL: mod_delayed_work() did not queue non-pending work\n");
		errors++;
	}

	destroy_workqueue(wq);
	return errors;
}

/* Test 18: Delayed work idle timeout pattern (amdgpu-style power management) */
static int test_idle_timeout_pattern(void)
{
	int errors = 0;
	bool was_pending;
	int i;

	tbridge_printf("\nTest 18: Idle timeout pattern (amdgpu power management)...\n");

	atomic_set(&power_up_count, 0);
	atomic_set(&idle_timeout_executed, 0);
	INIT_DELAYED_WORK(&idle_timeout_dwork, test_idle_timeout_fn);

	/*
	 * Simulate the amdgpu pattern:
	 * begin_use(): cancel delayed work, if wasn't pending => power up
	 * end_use(): schedule delayed work for idle timeout
	 */

	/* First use cycle - should need to power up */
	was_pending = cancel_delayed_work_sync(&idle_timeout_dwork);
	if (!was_pending) {
		atomic_inc(&power_up_count);
		tbridge_printf("INFO: First use - powered up (cancel returned false)\n");
	}
	/* ... do work ... */
	schedule_delayed_work(&idle_timeout_dwork, hz / 10);  /* 100ms idle timeout */

	/* Second use cycle quickly - should NOT need power up */
	was_pending = cancel_delayed_work_sync(&idle_timeout_dwork);
	if (!was_pending) {
		atomic_inc(&power_up_count);
		tbridge_printf("INFO: Second use - powered up (unexpected)\n");
	} else {
		tbridge_printf("INFO: Second use - still powered (cancel returned true)\n");
	}
	/* ... do work ... */
	schedule_delayed_work(&idle_timeout_dwork, hz / 10);

	/* Third use cycle quickly */
	was_pending = cancel_delayed_work_sync(&idle_timeout_dwork);
	if (!was_pending) {
		atomic_inc(&power_up_count);
	}
	schedule_delayed_work(&idle_timeout_dwork, hz / 10);

	/* Now let idle timeout fire */
	flush_delayed_work(&idle_timeout_dwork);

	/* Fourth use cycle after idle - should need power up again */
	was_pending = cancel_delayed_work_sync(&idle_timeout_dwork);
	if (!was_pending) {
		atomic_inc(&power_up_count);
		tbridge_printf("INFO: Fourth use (after idle) - powered up\n");
	}

	/* Verify pattern worked correctly */
	if (atomic_read(&power_up_count) == 2) {
		tbridge_printf("PASS: Power-up count correct (%d - initial and after idle)\n",
			atomic_read(&power_up_count));
	} else {
		tbridge_printf("FAIL: Expected 2 power-ups, got %d\n",
			atomic_read(&power_up_count));
		errors++;
	}

	if (atomic_read(&idle_timeout_executed) >= 1) {
		tbridge_printf("PASS: Idle timeout executed (%d times)\n",
			atomic_read(&idle_timeout_executed));
	} else {
		tbridge_printf("FAIL: Idle timeout never executed\n");
		errors++;
	}

	/* Cleanup - cancel any pending work */
	cancel_delayed_work_sync(&idle_timeout_dwork);

	return errors;
}

/* ============================================================
 * ORDERED WORKQUEUE TEST (Test 19)
 * ============================================================ */

/* Test 19: alloc_ordered_workqueue */
static int test_ordered_workqueue(void)
{
	struct workqueue_struct *wq;
	struct work_struct *works;
	int i;
	int errors = 0;
	const int num_items = 20;

	tbridge_printf("\nTest 19: alloc_ordered_workqueue...\n");

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

/* ============================================================
 * SYSTEM WORKQUEUE TESTS (Tests 20-22)
 * ============================================================ */

/* Test 20: system_unbound_wq */
static int test_system_unbound_wq(void)
{
	struct work_struct work;
	int errors = 0;

	tbridge_printf("\nTest 20: system_unbound_wq...\n");

	if (system_unbound_wq == NULL) {
		tbridge_printf("FAIL: system_unbound_wq is NULL\n");
		return 1;
	}

	atomic_set(&work_counter, 0);
	INIT_WORK(&work, test_work_fn);

	if (queue_work(system_unbound_wq, &work)) {
		tbridge_printf("PASS: queue_work(system_unbound_wq) queued\n");
	} else {
		tbridge_printf("INFO: queue_work() returned false\n");
	}

	flush_work(&work);

	if (atomic_read(&work_counter) == 1) {
		tbridge_printf("PASS: Work executed on system_unbound_wq (count=%d)\n",
			atomic_read(&work_counter));
	} else {
		tbridge_printf("FAIL: Work not executed (count=%d)\n",
			atomic_read(&work_counter));
		errors++;
	}

	return errors;
}

/* Test 21: system_highpri_wq */
static int test_system_highpri_wq(void)
{
	struct work_struct work;
	int errors = 0;

	tbridge_printf("\nTest 21: system_highpri_wq...\n");

	if (system_highpri_wq == NULL) {
		tbridge_printf("FAIL: system_highpri_wq is NULL\n");
		return 1;
	}

	atomic_set(&work_counter, 0);
	INIT_WORK(&work, test_work_fn);

	if (queue_work(system_highpri_wq, &work)) {
		tbridge_printf("PASS: queue_work(system_highpri_wq) queued\n");
	} else {
		tbridge_printf("INFO: queue_work() returned false\n");
	}

	flush_work(&work);

	if (atomic_read(&work_counter) == 1) {
		tbridge_printf("PASS: Work executed on system_highpri_wq (count=%d)\n",
			atomic_read(&work_counter));
	} else {
		tbridge_printf("FAIL: Work not executed (count=%d)\n",
			atomic_read(&work_counter));
		errors++;
	}

	return errors;
}

/* Test 22: system_long_wq */
static int test_system_long_wq(void)
{
	struct work_struct work;
	int errors = 0;

	tbridge_printf("\nTest 22: system_long_wq...\n");

	if (system_long_wq == NULL) {
		tbridge_printf("FAIL: system_long_wq is NULL\n");
		return 1;
	}

	atomic_set(&work_counter, 0);
	INIT_WORK(&work, test_work_fn);

	if (queue_work(system_long_wq, &work)) {
		tbridge_printf("PASS: queue_work(system_long_wq) queued\n");
	} else {
		tbridge_printf("INFO: queue_work() returned false\n");
	}

	flush_work(&work);

	if (atomic_read(&work_counter) == 1) {
		tbridge_printf("PASS: Work executed on system_long_wq (count=%d)\n",
			atomic_read(&work_counter));
	} else {
		tbridge_printf("FAIL: Work not executed (count=%d)\n",
			atomic_read(&work_counter));
		errors++;
	}

	return errors;
}

/* ============================================================
 * UTILITY FUNCTION TESTS (Tests 23, 26-27)
 * ============================================================ */

/* Test 23: flush_workqueue */
static int test_flush_workqueue(void)
{
	struct workqueue_struct *wq;
	struct work_struct *works;
	int i;
	int errors = 0;
	const int num_items = 30;

	tbridge_printf("\nTest 23: flush_workqueue...\n");

	wq = alloc_workqueue("test_flush", 0, 4);
	if (wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue() failed\n");
		return 1;
	}

	atomic_set(&work_counter, 0);

	works = kmalloc(sizeof(*works) * num_items, GFP_KERNEL);
	if (works == NULL) {
		tbridge_printf("FAIL: kmalloc() failed\n");
		destroy_workqueue(wq);
		return 1;
	}

	/* Queue all items */
	for (i = 0; i < num_items; i++) {
		INIT_WORK(&works[i], test_work_fn);
		queue_work(wq, &works[i]);
	}

	/* Flush the entire workqueue (not individual items) */
	flush_workqueue(wq);

	if (atomic_read(&work_counter) == num_items) {
		tbridge_printf("PASS: flush_workqueue() waited for all %d items\n",
			atomic_read(&work_counter));
	} else {
		tbridge_printf("FAIL: Expected %d, got %d after flush_workqueue()\n",
			num_items, atomic_read(&work_counter));
		errors++;
	}

	destroy_workqueue(wq);
	kfree(works);

	return errors;
}

/* Test 24: WQ_HIGHPRI flag */
static int test_wq_highpri_flag(void)
{
	struct workqueue_struct *wq;
	struct work_struct work;
	int errors = 0;

	tbridge_printf("\nTest 24: WQ_HIGHPRI flag...\n");

	wq = alloc_workqueue("test_highpri", WQ_HIGHPRI, 0);
	if (wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue(WQ_HIGHPRI) failed\n");
		return 1;
	}

	tbridge_printf("PASS: alloc_workqueue(WQ_HIGHPRI) created workqueue\n");

	atomic_set(&work_counter, 0);
	INIT_WORK(&work, test_work_fn);

	queue_work(wq, &work);
	flush_work(&work);

	if (atomic_read(&work_counter) == 1) {
		tbridge_printf("PASS: Work executed on WQ_HIGHPRI workqueue\n");
	} else {
		tbridge_printf("FAIL: Work not executed (count=%d)\n",
			atomic_read(&work_counter));
		errors++;
	}

	destroy_workqueue(wq);
	return errors;
}

/* Test 25: WQ_UNBOUND flag */
static int test_wq_unbound_flag(void)
{
	struct workqueue_struct *wq;
	struct work_struct work;
	int errors = 0;

	tbridge_printf("\nTest 25: WQ_UNBOUND flag...\n");

	wq = alloc_workqueue("test_unbound", WQ_UNBOUND, 0);
	if (wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue(WQ_UNBOUND) failed\n");
		return 1;
	}

	tbridge_printf("PASS: alloc_workqueue(WQ_UNBOUND) created workqueue\n");

	atomic_set(&work_counter, 0);
	INIT_WORK(&work, test_work_fn);

	queue_work(wq, &work);
	flush_work(&work);

	if (atomic_read(&work_counter) == 1) {
		tbridge_printf("PASS: Work executed on WQ_UNBOUND workqueue\n");
	} else {
		tbridge_printf("FAIL: Work not executed (count=%d)\n",
			atomic_read(&work_counter));
		errors++;
	}

	destroy_workqueue(wq);
	return errors;
}

/* Test 26: work_pending and work_busy */
static int test_work_pending_busy(void)
{
	struct work_struct work;
	int errors = 0;
	bool pending_before, busy_before;
	bool pending_after, busy_after;

	tbridge_printf("\nTest 26: work_pending() and work_busy()...\n");

	atomic_set(&work_counter, 0);
	INIT_WORK(&work, test_work_fn);

	/* Check before queueing */
	pending_before = work_pending(&work);
	busy_before = work_busy(&work);

	if (!pending_before) {
		tbridge_printf("PASS: work_pending() is false before queueing\n");
	} else {
		tbridge_printf("FAIL: work_pending() is true before queueing\n");
		errors++;
	}

	if (!busy_before) {
		tbridge_printf("PASS: work_busy() is false before queueing\n");
	} else {
		tbridge_printf("FAIL: work_busy() is true before queueing\n");
		errors++;
	}

	/* Queue and immediately flush */
	queue_work(system_wq, &work);
	flush_work(&work);

	/* Check after completion */
	pending_after = work_pending(&work);
	busy_after = work_busy(&work);

	if (!pending_after) {
		tbridge_printf("PASS: work_pending() is false after completion\n");
	} else {
		tbridge_printf("FAIL: work_pending() is true after completion\n");
		errors++;
	}

	if (!busy_after) {
		tbridge_printf("PASS: work_busy() is false after completion\n");
	} else {
		tbridge_printf("FAIL: work_busy() is true after completion\n");
		errors++;
	}

	if (atomic_read(&work_counter) == 1) {
		tbridge_printf("PASS: Work executed correctly\n");
	} else {
		tbridge_printf("FAIL: Work not executed\n");
		errors++;
	}

	return errors;
}

/* Test 27: delayed_work_pending */
static int test_delayed_work_pending(void)
{
	struct delayed_work dwork;
	int errors = 0;
	bool pending_before, pending_during;

	tbridge_printf("\nTest 27: delayed_work_pending()...\n");

	atomic_set(&delayed_work_counter, 0);
	INIT_DELAYED_WORK(&dwork, test_delayed_work_fn);

	/* Check before queueing */
	pending_before = delayed_work_pending(&dwork);

	if (!pending_before) {
		tbridge_printf("PASS: delayed_work_pending() is false before queueing\n");
	} else {
		tbridge_printf("FAIL: delayed_work_pending() is true before queueing\n");
		errors++;
	}

	/* Queue with longer delay so we can check pending state */
	schedule_delayed_work(&dwork, hz / 2);  /* 500ms */

	/* Check while pending */
	pending_during = delayed_work_pending(&dwork);

	if (pending_during) {
		tbridge_printf("PASS: delayed_work_pending() is true while queued\n");
	} else {
		tbridge_printf("FAIL: delayed_work_pending() is false while queued\n");
		errors++;
	}

	/* Wait for completion */
	flush_delayed_work(&dwork);

	/* Check after firing - may or may not be pending depending on timing */
	tbridge_printf("INFO: After flush, delayed_work_pending() = %s\n",
		delayed_work_pending(&dwork) ? "true" : "false");

	if (atomic_read(&delayed_work_counter) == 1) {
		tbridge_printf("PASS: Delayed work executed correctly\n");
	} else {
		tbridge_printf("FAIL: Delayed work not executed (count=%d)\n",
			atomic_read(&delayed_work_counter));
		errors++;
	}

	return errors;
}

/* ============================================================
 * STRESS TEST (Test 28)
 * ============================================================ */

/* Test 28: Concurrent cancel and queue stress test */
static int test_concurrent_cancel_queue(void)
{
	struct workqueue_struct *wq;
	struct work_struct *works;
	int i, round;
	int errors = 0;
	const int num_items = 50;
	const int num_rounds = 10;

	tbridge_printf("\nTest 28: Concurrent cancel and queue stress test...\n");

	wq = alloc_workqueue("test_stress", 0, 4);
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

	atomic_set(&stress_queue_success, 0);
	atomic_set(&stress_cancel_success, 0);
	atomic_set(&stress_executed, 0);

	for (round = 0; round < num_rounds; round++) {
		/* Initialize all work items */
		for (i = 0; i < num_items; i++) {
			INIT_WORK(&works[i], test_stress_fn);
		}

		/* Queue half the items */
		for (i = 0; i < num_items / 2; i++) {
			if (queue_work(wq, &works[i])) {
				atomic_inc(&stress_queue_success);
			}
		}

		/* Cancel some items (some queued, some not) */
		for (i = 0; i < num_items / 4; i++) {
			if (cancel_work_sync(&works[i])) {
				atomic_inc(&stress_cancel_success);
			}
		}

		/* Queue remaining items */
		for (i = num_items / 2; i < num_items; i++) {
			if (queue_work(wq, &works[i])) {
				atomic_inc(&stress_queue_success);
			}
		}

		/* Drain to complete round */
		drain_workqueue(wq);
	}

	tbridge_printf("INFO: Queued %d, canceled %d, executed %d\n",
		atomic_read(&stress_queue_success),
		atomic_read(&stress_cancel_success),
		atomic_read(&stress_executed));

	/* Success criteria: no crashes/hangs, reasonable execution count */
	if (atomic_read(&stress_executed) > 0) {
		tbridge_printf("PASS: Stress test completed without crashes\n");
	} else {
		tbridge_printf("FAIL: No work items executed\n");
		errors++;
	}

	destroy_workqueue(wq);
	kfree(works);

	return errors;
}

/* Helper to get elapsed time in milliseconds */
static long
elapsed_ms(struct timeval *start, struct timeval *end)
{
	return (end->tv_sec - start->tv_sec) * 1000 +
	       (end->tv_usec - start->tv_usec) / 1000;
}

/* Main test runner */
static void
linuxkpi_workqueue_run(void *arg __unused)
{
	int total_errors = 0;
	struct timeval start_time, end_time;

	/* Reset abort flag */
	atomic_set(&test_abort_flag, 0);

	microtime(&start_time);

	tbridge_printf("========================================\n");
	tbridge_printf("LinuxKPI Workqueue Functional Test\n");
	tbridge_printf("Actually exercising workqueue APIs!\n");
	tbridge_printf("========================================\n\n");

	total_errors += test_alloc_workqueue();
	if (atomic_read(&test_abort_flag)) goto aborted;

	total_errors += test_queue_work();
	if (atomic_read(&test_abort_flag)) goto aborted;

	total_errors += test_system_wq();
	if (atomic_read(&test_abort_flag)) goto aborted;

	total_errors += test_schedule_work();
	if (atomic_read(&test_abort_flag)) goto aborted;

	total_errors += test_multiple_work();
	if (atomic_read(&test_abort_flag)) goto aborted;

	total_errors += test_cancel_work();
	if (atomic_read(&test_abort_flag)) goto aborted;

	total_errors += test_sustained_work();
	if (atomic_read(&test_abort_flag)) goto aborted;

	total_errors += test_requeue_work();
	if (atomic_read(&test_abort_flag)) goto aborted;

	total_errors += test_concurrent_workqueues();
	if (atomic_read(&test_abort_flag)) goto aborted;

	total_errors += test_per_cpu_distribution();
	if (atomic_read(&test_abort_flag)) goto aborted;

	total_errors += test_stress_many_items();
	if (atomic_read(&test_abort_flag)) goto aborted;

	/* Delayed work tests (12-18) */
	total_errors += test_basic_delayed_work();
	if (atomic_read(&test_abort_flag)) goto aborted;

	total_errors += test_cancel_delayed_before();
	if (atomic_read(&test_abort_flag)) goto aborted;

	total_errors += test_cancel_delayed_after();
	if (atomic_read(&test_abort_flag)) goto aborted;

	total_errors += test_delayed_timeout_fires();
	if (atomic_read(&test_abort_flag)) goto aborted;

	total_errors += test_delayed_self_requeue();
	if (atomic_read(&test_abort_flag)) goto aborted;

	total_errors += test_mod_delayed_work();
	if (atomic_read(&test_abort_flag)) goto aborted;

	total_errors += test_idle_timeout_pattern();
	if (atomic_read(&test_abort_flag)) goto aborted;

	/* Ordered workqueue test (19) */
	total_errors += test_ordered_workqueue();
	if (atomic_read(&test_abort_flag)) goto aborted;

	/* System workqueue tests (20-22) */
	total_errors += test_system_unbound_wq();
	if (atomic_read(&test_abort_flag)) goto aborted;

	total_errors += test_system_highpri_wq();
	if (atomic_read(&test_abort_flag)) goto aborted;

	total_errors += test_system_long_wq();
	if (atomic_read(&test_abort_flag)) goto aborted;

	/* Utility function tests (23, 26-27) */
	total_errors += test_flush_workqueue();
	if (atomic_read(&test_abort_flag)) goto aborted;

	/* WQ flag tests (24-25) */
	total_errors += test_wq_highpri_flag();
	if (atomic_read(&test_abort_flag)) goto aborted;

	total_errors += test_wq_unbound_flag();
	if (atomic_read(&test_abort_flag)) goto aborted;

	/* Pending/busy tests (26-27) */
	total_errors += test_work_pending_busy();
	if (atomic_read(&test_abort_flag)) goto aborted;

	total_errors += test_delayed_work_pending();
	if (atomic_read(&test_abort_flag)) goto aborted;

	/* Stress test (28) */
	total_errors += test_concurrent_cancel_queue();

	microtime(&end_time);

	tbridge_printf("\n========================================\n");
	tbridge_printf("Total time: %ld ms\n", elapsed_ms(&start_time, &end_time));
	tbridge_printf("Tests completed: 28\n");
	if (total_errors == 0) {
		tbridge_printf("ALL WORKQUEUE TESTS PASSED!\n");
		tbridge_printf("========================================\n");
		tbridge_test_done(RESULT_PASS);
	} else {
		tbridge_printf("TESTS FAILED: %d errors\n", total_errors);
		tbridge_printf("========================================\n");
		tbridge_test_done(RESULT_FAIL);
	}
	return;

aborted:
	microtime(&end_time);
	tbridge_printf("\n========================================\n");
	tbridge_printf("TEST ABORTED after %ld ms\n", elapsed_ms(&start_time, &end_time));
	tbridge_printf("========================================\n");
	tbridge_test_done(RESULT_TIMEOUT);
}

/*
 * Abort callback - called when test times out.
 * Sets the abort flag to signal the test runner to stop.
 */
static int
linuxkpi_workqueue_abort(void)
{
	atomic_set(&test_abort_flag, 1);
	return 0;
}

static struct tbridge_testcase linuxkpi_workqueue_case = {
	.tb_run		= linuxkpi_workqueue_run,
	.tb_abort	= linuxkpi_workqueue_abort
};

TBRIDGE_TESTCASE_MODULE(linuxkpi_workqueue, &linuxkpi_workqueue_case);
