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
 * Test 32: INIT_DELAYED_WORK_ONSTACK
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t onstack_count = ATOMIC_INIT(0);

static void
test_onstack_fn(struct work_struct *work)
{
\tatomic_inc(&onstack_count);
}

static int
wq_test32_run(void)
{
\tstruct delayed_work dwork;
\tint errors = 0;

\ttbridge_printf("Test: INIT_DELAYED_WORK_ONSTACK...\n");

\tatomic_set(&onstack_count, 0);
\tINIT_DELAYED_WORK_ONSTACK(&dwork, test_onstack_fn);

\tif (!schedule_delayed_work(&dwork, hz / 20))
\t\ttbridge_printf("INFO: schedule_delayed_work() returned false\n");

\tflush_delayed_work(&dwork);

\tif (atomic_read(&onstack_count) == 1) {
\t\ttbridge_printf("PASS: on-stack delayed work executed\n");
\t} else {
\t\ttbridge_printf("FAIL: expected 1 execution, got %d\n",
\t\t    atomic_read(&onstack_count));
\t\terrors++;
\t}

\tcancel_delayed_work_sync(&dwork);
\tdestroy_delayed_work_on_stack(&dwork);

\treturn errors;
}

DEFINE_WQ_TEST(wq_test32, "INIT_DELAYED_WORK_ONSTACK");
