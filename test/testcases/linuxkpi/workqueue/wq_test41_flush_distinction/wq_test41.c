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
 * Test 41: flush_delayed_work vs flush_work distinction
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t work_count = ATOMIC_INIT(0);
static struct delayed_work test_dwork;

static void
test_work_fn(struct work_struct *work)
{
    atomic_inc(&work_count);
}

static int
wq_test41_run(void)
{
    int errors = 0;
    bool flushed;

    tbridge_printf("Test: flush_delayed_work vs flush_work...\n");

    INIT_DELAYED_WORK(&test_dwork, test_work_fn);
    atomic_set(&work_count, 0);

    schedule_delayed_work(&test_dwork, hz * 5);

    flushed = flush_work(&test_dwork.work);
    if (flushed) {
        tbridge_printf("INFO: flush_work reported busy\n");
    }

    if (atomic_read(&work_count) != 0) {
        tbridge_printf("FAIL: flush_work executed delayed work early\n");
        errors++;
    }

    if (!delayed_work_pending(&test_dwork)) {
        tbridge_printf("FAIL: delayed work not pending after flush_work\n");
        errors++;
    }

    flush_delayed_work(&test_dwork);

    if (atomic_read(&work_count) == 1) {
        tbridge_printf("PASS: flush_delayed_work waited for execution\n");
    } else {
        tbridge_printf("FAIL: expected 1 execution, got %d\n",
            atomic_read(&work_count));
        errors++;
    }

    cancel_delayed_work_sync(&test_dwork);
    return errors;
}

DEFINE_WQ_TEST(wq_test41, "flush_delayed_work vs flush_work");
