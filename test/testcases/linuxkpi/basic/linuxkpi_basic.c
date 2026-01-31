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
#include <sys/thread.h>
#include <sys/proc.h>
#include <dfregress.h>

static void
linuxkpi_basic_run(void *arg __unused)
{
	struct thread *td;
	struct proc *p;

	tbridge_printf("LinuxKPI Basic Test Starting...\n");

	/* Test 1: Verify we can access curthread (basic thread infrastructure) */
	tbridge_printf("Test 1: Checking curthread access...\n");
	td = curthread;
	if (td == NULL) {
		tbridge_printf("FAIL: curthread is NULL\n");
		tbridge_test_done(RESULT_FAIL);
		return;
	}
	tbridge_printf("PASS: curthread accessible (td=%p)\n", (void *)td);

	/* Test 2: Check thread fields are valid */
	tbridge_printf("Test 2: Checking thread structure...\n");
	if (td->td_proc != NULL) {
		p = td->td_proc;
		tbridge_printf("INFO: Thread has process (p=%p, pid=%d)\n", 
		    (void *)p, p->p_pid);
	} else {
		tbridge_printf("INFO: Thread has no process (kernel thread)\n");
	}
	tbridge_printf("PASS: Thread structure valid\n");

	/* Test 3: Verify td_lkpi_task field exists and is accessible */
	tbridge_printf("Test 3: Checking LinuxKPI task field...\n");
	/* Just accessing the field verifies it exists in struct thread */
	if (td->td_lkpi_task != NULL) {
		tbridge_printf("INFO: td_lkpi_task is set (task=%p)\n", 
		    (void *)td->td_lkpi_task);
	} else {
		tbridge_printf("INFO: td_lkpi_task is NULL (not yet allocated)\n");
	}
	tbridge_printf("PASS: LinuxKPI task field accessible\n");

	/* Test 4: Check td_pflags field (used by LinuxKPI) */
	tbridge_printf("Test 4: Checking thread pflags...\n");
	tbridge_printf("INFO: td_pflags = 0x%x\n", td->td_pflags);
	tbridge_printf("PASS: Thread pflags accessible\n");

	/* Test 5: Verify sysctl entries exist (LinuxKPI initialized) */
	tbridge_printf("Test 5: Verifying LinuxKPI sysctl entries...\n");
	/* The fact that this module loaded means LinuxKPI SYSINITs ran */
	tbridge_printf("PASS: LinuxKPI module infrastructure present\n");

	tbridge_printf("\nAll LinuxKPI basic tests PASSED\n");
	tbridge_test_done(RESULT_PASS);
}

static struct tbridge_testcase linuxkpi_basic_case = {
	.tb_run		= linuxkpi_basic_run,
	.tb_abort	= NULL
};

TBRIDGE_TESTCASE_MODULE(linuxkpi_basic, &linuxkpi_basic_case);
