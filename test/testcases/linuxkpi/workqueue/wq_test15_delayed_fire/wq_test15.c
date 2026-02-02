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
 * Test 15: Delayed work timeout fires
 *
 * Tests delayed work on custom workqueue:
 * - Create custom workqueue
 * - Queue delayed work with queue_delayed_work
 * - Verify callback executed after timeout
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t delayed_work_counter = ATOMIC_INIT(0);

static void
test_delayed_work_fn(struct work_struct *work)
{
	atomic_inc(&delayed_work_counter);
}

static int
wq_test15_run(void)
{
	struct workqueue_struct *wq;
	struct delayed_work dwork;
	int errors = 0;

	tbridge_printf("Test: Delayed work timeout fires...\n");

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

	/* Ensure work is fully cancelled before destroying workqueue */
	cancel_delayed_work_sync(&dwork);
	destroy_workqueue(wq);

	return errors;
}

DEFINE_WQ_TEST(wq_test15, "Delayed work timeout fires");
