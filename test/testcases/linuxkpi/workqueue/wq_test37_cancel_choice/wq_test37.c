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
 * Test 37: Conditional sync vs non-sync cancel
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t work_executed = ATOMIC_INIT(0);
static struct delayed_work test_dwork;

static void
test_work_fn(struct work_struct *work)
{
    atomic_inc(&work_executed);
}

static int
wq_test37_run(void)
{
    int errors = 0;
    bool was_pending;

    tbridge_printf("Test: conditional sync vs non-sync cancel...\n");

    INIT_DELAYED_WORK(&test_dwork, test_work_fn);

\t/* Subtest 1: use non-sync cancel */
    atomic_set(&work_executed, 0);
    schedule_delayed_work(&test_dwork, hz * 5);
    pause("wqwait", hz / 100);

    was_pending = cancel_delayed_work(&test_dwork);
    if (was_pending) {
        tbridge_printf("PASS: non-sync cancel returned true\n");
    } else {
        tbridge_printf("FAIL: non-sync cancel returned false for pending\n");
        errors++;
    }
    cancel_delayed_work_sync(&test_dwork);

\t/* Subtest 2: use sync cancel */
    atomic_set(&work_executed, 0);
    schedule_delayed_work(&test_dwork, hz * 5);
    pause("wqwait", hz / 100);

    was_pending = cancel_delayed_work_sync(&test_dwork);
    if (was_pending) {
        tbridge_printf("PASS: sync cancel returned true\n");
    } else {
        tbridge_printf("FAIL: sync cancel returned false for pending\n");
        errors++;
    }

\t/* Subtest 3: non-pending should return false */
    was_pending = cancel_delayed_work_sync(&test_dwork);
    if (!was_pending) {
        tbridge_printf("PASS: sync cancel returned false for non-pending\n");
    } else {
        tbridge_printf("FAIL: sync cancel returned true for non-pending\n");
        errors++;
    }

    return errors;
}

DEFINE_WQ_TEST(wq_test37, "conditional sync vs non-sync cancel");
