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
 * Test 14: cancel_delayed_work_sync after firing
 *
 * Tests canceling delayed work after its timer has fired and callback executed:
 * - Queue delayed work with short delay (50ms)
 * - Wait for execution with flush_delayed_work
 * - Then cancel - should return false since already executed
 * - Verify callback WAS executed
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t delayed_work_counter = ATOMIC_INIT(0);

static void
test_delayed_work_fn(struct work_struct *work)
{
	atomic_inc(&delayed_work_counter);
}

static int
wq_test14_run(void)
{
	struct delayed_work dwork;
	bool was_pending;
	int errors = 0;

	tbridge_printf("Test: cancel_delayed_work_sync (after firing)...\n");

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

DEFINE_WQ_TEST(wq_test14, "cancel_delayed_work_sync after firing");
