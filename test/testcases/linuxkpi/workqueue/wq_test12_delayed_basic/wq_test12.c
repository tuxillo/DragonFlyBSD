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
 * Test 12: Basic delayed work
 *
 * Tests basic delayed work functionality:
 * - INIT_DELAYED_WORK to initialize
 * - schedule_delayed_work to queue with delay
 * - flush_delayed_work to wait for completion
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t delayed_work_counter = ATOMIC_INIT(0);

static void
test_delayed_work_fn(struct work_struct *work)
{
	atomic_inc(&delayed_work_counter);
}

static int
wq_test12_run(void)
{
	struct delayed_work dwork;
	int errors = 0;

	tbridge_printf("Test: Basic delayed work...\n");

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

	/* Ensure work is fully cancelled before stack variable goes out of scope */
	cancel_delayed_work_sync(&dwork);

	return errors;
}

DEFINE_WQ_TEST(wq_test12, "Basic delayed work");
