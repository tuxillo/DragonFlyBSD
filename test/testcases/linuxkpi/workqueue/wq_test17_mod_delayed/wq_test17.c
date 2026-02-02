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
 * Test 17: mod_delayed_work
 *
 * Tests modifying the delay of pending delayed work:
 * - Queue with long delay (5 seconds)
 * - Modify to short delay (100ms)
 * - Verify fires quickly (not the original 5 seconds)
 * - Also test mod_delayed_work on non-pending work (should queue it)
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t delayed_work_counter = ATOMIC_INIT(0);

static void
test_delayed_work_fn(struct work_struct *work)
{
	atomic_inc(&delayed_work_counter);
}

static int
wq_test17_run(void)
{
	struct workqueue_struct *wq;
	struct delayed_work dwork;
	struct timeval start, end;
	long elapsed;
	bool was_pending;
	int errors = 0;

	tbridge_printf("Test: mod_delayed_work...\n");

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

	elapsed = wq_elapsed_ms(&start, &end);

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

	/* Ensure work is fully cancelled before destroying workqueue */
	cancel_delayed_work_sync(&dwork);
	destroy_workqueue(wq);

	return errors;
}

DEFINE_WQ_TEST(wq_test17, "mod_delayed_work");
