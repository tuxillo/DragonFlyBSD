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
 * Test 42: cancel_delayed_work_sync loop pattern
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t loop_count = ATOMIC_INIT(0);
static struct delayed_work loop_dwork;

static void
test_loop_fn(struct work_struct *work)
{
\tint count = atomic_inc_return(&loop_count);

\tif (count < 3)
\t\tschedule_delayed_work(&loop_dwork, hz / 50);
}

static int
wq_test42_run(void)
{
\tint errors = 0;
\tint loops = 0;

\ttbridge_printf("Test: cancel_delayed_work_sync loop pattern...\n");

\tatomic_set(&loop_count, 0);
\tINIT_DELAYED_WORK(&loop_dwork, test_loop_fn);

\tschedule_delayed_work(&loop_dwork, hz / 50);
\tpause("wqwait", hz / 20);

\twhile (cancel_delayed_work_sync(&loop_dwork)) {
\t\tloops++;
\t\tif (loops > 10) {
\t\t\ttbridge_printf("FAIL: cancel loop did not converge\n");
\t\t\terrors++;
\t\t\tbreak;
\t\t}
\t}

\tif (loops >= 1) {
\t\ttbridge_printf("PASS: cancel loop completed after %d iterations\n", loops);
\t} else {
\t\ttbridge_printf("FAIL: cancel loop did not cancel pending work\n");
\t\terrors++;
\t}

\tif (delayed_work_pending(&loop_dwork)) {
\t\ttbridge_printf("FAIL: delayed work still pending after loop\n");
\t\terrors++;
\t}

\treturn errors;
}

DEFINE_WQ_TEST(wq_test42, "cancel_delayed_work_sync loop pattern");
