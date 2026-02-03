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
 * Test 36: cancel_delayed_work (non-sync) conditional cleanup
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t work_executed = ATOMIC_INIT(0);
static atomic_t cleanup_count = ATOMIC_INIT(0);
static struct delayed_work test_dwork;

static void
test_work_fn(struct work_struct *work)
{
\tatomic_inc(&work_executed);
}

static int
wq_test36_run(void)
{
\tint errors = 0;
\tbool was_pending;

\ttbridge_printf("Test: cancel_delayed_work (non-sync) conditional cleanup...\n");

\tINIT_DELAYED_WORK(&test_dwork, test_work_fn);

\t/* Subtest 1: cancel pending work should return true and cleanup */
\tatomic_set(&work_executed, 0);
\tatomic_set(&cleanup_count, 0);
\tschedule_delayed_work(&test_dwork, hz * 5);
\tpause("wqwait", hz / 100);

\twas_pending = cancel_delayed_work(&test_dwork);
\tif (was_pending)
\t\tatomic_inc(&cleanup_count);

\tif (was_pending) {
\t\ttbridge_printf("PASS: cancel_delayed_work returned true for pending\n");
\t} else {
\t\ttbridge_printf("FAIL: cancel_delayed_work returned false for pending\n");
\t\terrors++;
\t}

\tif (atomic_read(&cleanup_count) == 1) {
\t\ttbridge_printf("PASS: cleanup executed when cancelled\n");
\t} else {
\t\ttbridge_printf("FAIL: cleanup not executed as expected\n");
\t\terrors++;
\t}

\tcancel_delayed_work_sync(&test_dwork);

\tif (atomic_read(&work_executed) != 0) {
\t\ttbridge_printf("FAIL: work executed despite cancel\n");
\t\terrors++;
\t}

\t/* Subtest 2: cancel non-pending work should return false */
\tatomic_set(&cleanup_count, 0);
\twas_pending = cancel_delayed_work(&test_dwork);

\tif (!was_pending) {
\t\ttbridge_printf("PASS: cancel_delayed_work returned false for non-pending\n");
\t} else {
\t\ttbridge_printf("FAIL: cancel_delayed_work returned true for non-pending\n");
\t\terrors++;
\t}

\tif (atomic_read(&cleanup_count) == 0) {
\t\ttbridge_printf("PASS: cleanup not executed for non-pending\n");
\t} else {
\t\ttbridge_printf("FAIL: cleanup executed unexpectedly\n");
\t\terrors++;
\t}

\treturn errors;
}

DEFINE_WQ_TEST(wq_test36, "cancel_delayed_work non-sync conditional cleanup");
