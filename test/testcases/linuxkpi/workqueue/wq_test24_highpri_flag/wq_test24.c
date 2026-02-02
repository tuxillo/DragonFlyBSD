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
 * Test 24: WQ_HIGHPRI flag
 *
 * Tests alloc_workqueue with WQ_HIGHPRI flag:
 * - Create workqueue with WQ_HIGHPRI
 * - Queue work and verify execution
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t work_counter = ATOMIC_INIT(0);

static void
test_work_fn(struct work_struct *work)
{
	atomic_inc(&work_counter);
}

static int
wq_test24_run(void)
{
	struct workqueue_struct *wq;
	struct work_struct work;
	int errors = 0;

	tbridge_printf("Test: alloc_workqueue() with WQ_HIGHPRI flag...\n");

	wq = alloc_workqueue("test_highpri", WQ_HIGHPRI, 0);
	if (wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue(WQ_HIGHPRI) failed\n");
		return 1;
	}
	tbridge_printf("PASS: alloc_workqueue(WQ_HIGHPRI) succeeded\n");

	atomic_set(&work_counter, 0);
	INIT_WORK(&work, test_work_fn);

	if (queue_work(wq, &work)) {
		tbridge_printf("PASS: queue_work() to highpri wq succeeded\n");
	} else {
		tbridge_printf("INFO: queue_work() returned false\n");
	}

	flush_work(&work);

	if (atomic_read(&work_counter) == 1) {
		tbridge_printf("PASS: work callback executed (count=%d)\n",
		    atomic_read(&work_counter));
	} else {
		tbridge_printf("FAIL: work callback not executed (count=%d)\n",
		    atomic_read(&work_counter));
		errors++;
	}

	destroy_workqueue(wq);
	tbridge_printf("PASS: destroy_workqueue() succeeded\n");

	return errors;
}

DEFINE_WQ_TEST(wq_test24, "WQ_HIGHPRI flag");
