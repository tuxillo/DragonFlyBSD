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
 * Test 06: cancel_work_sync
 *
 * Tests synchronous work cancellation:
 * - Queue work to system_wq
 * - Immediately call cancel_work_sync()
 * - Verify no crash/hang
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t work_counter = ATOMIC_INIT(0);

static void
test_work_fn(struct work_struct *work)
{
	atomic_inc(&work_counter);
}

static int
wq_test06_run(void)
{
	struct work_struct work;
	int errors = 0;

	tbridge_printf("Test: cancel_work_sync()...\n");

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

DEFINE_WQ_TEST(wq_test06, "cancel_work_sync");
