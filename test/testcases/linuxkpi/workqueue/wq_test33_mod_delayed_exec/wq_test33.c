/*-
 * Copyright (c) 2025
 *\tThe DragonFly Project.  All rights reserved.
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
 * Test 33: mod_delayed_work on executing work
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t exec_count = ATOMIC_INIT(0);
static struct delayed_work exec_dwork;

static void
test_exec_mod_fn(struct work_struct *work)
{
\tint count = atomic_inc_return(&exec_count);

\tif (count == 1)
\t\tmod_delayed_work(system_wq, &exec_dwork, hz / 20);
}

static int
wq_test33_run(void)
{
\tint errors = 0;
\tint loops = 0;

\ttbridge_printf("Test: mod_delayed_work while executing...\n");

\tatomic_set(&exec_count, 0);
\tINIT_DELAYED_WORK(&exec_dwork, test_exec_mod_fn);

\tqueue_delayed_work(system_wq, &exec_dwork, hz / 20);

\twhile (atomic_read(&exec_count) < 2 && loops < 200) {
\t\tpause("wqwait", hz / 100);
\t\tloops++;
\t}

\tif (atomic_read(&exec_count) == 2) {
\t\ttbridge_printf("PASS: mod_delayed_work rescheduled executing work\n");
\t} else {
\t\ttbridge_printf("FAIL: expected 2 executions, got %d\n",
\t\t    atomic_read(&exec_count));
\t\terrors++;
\t}

\tcancel_delayed_work_sync(&exec_dwork);

\treturn errors;
}

DEFINE_WQ_TEST(wq_test33, "mod_delayed_work on executing work");
