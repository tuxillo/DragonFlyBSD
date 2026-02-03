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
 * Test 43: Multiple workqueue flush sequence
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t wq1_count = ATOMIC_INIT(0);
static atomic_t wq2_count = ATOMIC_INIT(0);
static atomic_t wq3_count = ATOMIC_INIT(0);

static void
test_wq1_fn(struct work_struct *work)
{
\tatomic_inc(&wq1_count);
}

static void
test_wq2_fn(struct work_struct *work)
{
\tatomic_inc(&wq2_count);
}

static void
test_wq3_fn(struct work_struct *work)
{
\tatomic_inc(&wq3_count);
}

static int
wq_test43_run(void)
{
\tstruct workqueue_struct *wq1;
\tstruct workqueue_struct *wq2;
\tstruct workqueue_struct *wq3;
\tstruct work_struct work1, work2, work3;
\tint errors = 0;

\ttbridge_printf("Test: multiple workqueue flush sequence...\n");

\twq1 = alloc_ordered_workqueue("wq1", 0);
\twq2 = alloc_workqueue("wq2", WQ_UNBOUND, 0);
\twq3 = alloc_workqueue("wq3", WQ_HIGHPRI, 0);
\tif (wq1 == NULL || wq2 == NULL || wq3 == NULL) {
\t\ttbridge_printf("FAIL: alloc_workqueue() failed\n");
\t\tif (wq1)
\t\t\tdestroy_workqueue(wq1);
\t\tif (wq2)
\t\t\tdestroy_workqueue(wq2);
\t\tif (wq3)
\t\t\tdestroy_workqueue(wq3);
\t\treturn 1;
\t}

\tatomic_set(&wq1_count, 0);
\tatomic_set(&wq2_count, 0);
\tatomic_set(&wq3_count, 0);

\tINIT_WORK(&work1, test_wq1_fn);
\tINIT_WORK(&work2, test_wq2_fn);
\tINIT_WORK(&work3, test_wq3_fn);

\tqueue_work(wq1, &work1);
\tqueue_work(wq2, &work2);
\tqueue_work(wq3, &work3);

\tflush_workqueue(wq1);
\tflush_workqueue(wq2);
\tflush_workqueue(wq3);

\tif (atomic_read(&wq1_count) != 1 || atomic_read(&wq2_count) != 1 ||
\t    atomic_read(&wq3_count) != 1) {
\t\ttbridge_printf("FAIL: counts wq1=%d wq2=%d wq3=%d\n",
\t\t    atomic_read(&wq1_count), atomic_read(&wq2_count),
\t\t    atomic_read(&wq3_count));
\t\terrors++;
\t} else {
\t\ttbridge_printf("PASS: all workqueues flushed in sequence\n");
\t}

\tdestroy_workqueue(wq1);
\tdestroy_workqueue(wq2);
\tdestroy_workqueue(wq3);
\treturn errors;
}

DEFINE_WQ_TEST(wq_test43, "multiple workqueue flush sequence");
