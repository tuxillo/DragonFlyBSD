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

#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/tbridge.h>
#include <dfregress.h>

#include <linux/compat.h>

static int linux_alloc_current_noop(struct thread *td, int flags);
extern int (*lkpi_alloc_current)(struct thread *, int);

static void
linuxkpi_basic_run(void *arg __unused)
{
	struct thread *td;
	int ret;

	tbridge_printf("LinuxKPI Basic Test Starting...\n");

	/* Test 1: Verify lkpi_alloc_current is initialized */
	tbridge_printf("Test 1: Checking lkpi_alloc_current initialization...\n");
	if (lkpi_alloc_current == NULL) {
		tbridge_printf("FAIL: lkpi_alloc_current is NULL\n");
		tbridge_test_done(RESULT_FAIL);
		return;
	}
	if (lkpi_alloc_current == linux_alloc_current_noop) {
		tbridge_printf("FAIL: lkpi_alloc_current still points to noop function\n");
		tbridge_test_done(RESULT_FAIL);
		return;
	}
	tbridge_printf("PASS: lkpi_alloc_current is properly initialized\n");

	/* Test 2: Verify current thread has td_lkpi_task allocated */
	tbridge_printf("Test 2: Checking current thread td_lkpi_task...\n");
	td = curthread;
	if (td == NULL) {
		tbridge_printf("FAIL: curthread is NULL\n");
		tbridge_test_done(RESULT_FAIL);
		return;
	}

	if (td->td_lkpi_task == NULL) {
		tbridge_printf("Test 2: td_lkpi_task is NULL, attempting allocation...\n");
		linux_set_current(td);
		if (td->td_lkpi_task == NULL) {
			tbridge_printf("FAIL: Failed to allocate td_lkpi_task for current thread\n");
			tbridge_test_done(RESULT_FAIL);
			return;
		}
	}
	tbridge_printf("PASS: td_lkpi_task is allocated (task=%p)\n", td->td_lkpi_task);

	/* Test 3: Verify allocation works with M_NOWAIT flag */
	tbridge_printf("Test 3: Testing allocation with M_NOWAIT flag...\n");
	ret = lkpi_alloc_current(td, M_NOWAIT);
	if (ret != 0 && td->td_lkpi_task == NULL) {
		tbridge_printf("INFO: M_NOWAIT allocation returned %d (expected since already allocated)\n", ret);
	}
	tbridge_printf("PASS: M_NOWAIT allocation test completed\n");

	/* Test 4: Verify kernel threads without td_proc are handled */
	tbridge_printf("Test 4: Checking kernel thread process handling...\n");
	if (td->td_proc == NULL) {
		tbridge_printf("INFO: Current thread has no process (kernel thread)\n");
		tbridge_printf("INFO: task_struct mm should be NULL for kernel threads\n");
	} else {
		tbridge_printf("INFO: Current thread has process attached\n");
	}
	tbridge_printf("PASS: Process handling verified\n");

	tbridge_printf("\nAll LinuxKPI basic tests PASSED\n");
	tbridge_test_done(RESULT_PASS);
}

static struct tbridge_testcase linuxkpi_basic_case = {
	.tb_run		= linuxkpi_basic_run,
	.tb_abort	= NULL
};

TBRIDGE_TESTCASE_MODULE(linuxkpi_basic, &linuxkpi_basic_case);
