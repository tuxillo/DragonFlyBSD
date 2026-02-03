/*-
 * Copyright (c) 2025
 *    The DragonFly Project.  All rights reserved.
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
 * Test 39: Gated work queueing
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t gated_count = ATOMIC_INIT(0);
static struct delayed_work gated_dwork;
static bool gated_enabled;

static void
test_gated_fn(struct work_struct *work)
{
    atomic_inc(&gated_count);
}

static bool
gated_queue(struct workqueue_struct *wq, struct delayed_work *dwork, unsigned long delay)
{
    if (!gated_enabled)
        return (false);
    return mod_delayed_work(wq, dwork, delay);
}

static int
wq_test39_run(void)
{
    int errors = 0;

    tbridge_printf("Test: gated work queueing...\n");

    INIT_DELAYED_WORK(&gated_dwork, test_gated_fn);

    /* Disabled gate */
    gated_enabled = false;
    atomic_set(&gated_count, 0);
    if (gated_queue(system_wq, &gated_dwork, hz / 10)) {
        tbridge_printf("FAIL: gated queue returned true while disabled\n");
        errors++;
    } else {
        tbridge_printf("PASS: gated queue returned false while disabled\n");
    }

    pause("wqwait", hz / 10);
    if (atomic_read(&gated_count) != 0) {
        tbridge_printf("FAIL: work executed while gated\n");
        errors++;
    }

    /* Enabled gate */
    gated_enabled = true;
    if (!gated_queue(system_wq, &gated_dwork, hz / 10))
        tbridge_printf("INFO: gated queue returned false while enabled\n");

    flush_delayed_work(&gated_dwork);

    if (atomic_read(&gated_count) == 1) {
        tbridge_printf("PASS: work executed when gate enabled\n");
    } else {
        tbridge_printf("FAIL: expected 1 execution, got %d\n",
            atomic_read(&gated_count));
        errors++;
    }

    cancel_delayed_work_sync(&gated_dwork);
    return errors;
}

DEFINE_WQ_TEST(wq_test39, "gated work queueing");
