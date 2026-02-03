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
    atomic_inc(&wq1_count);
}

static void
test_wq2_fn(struct work_struct *work)
{
    atomic_inc(&wq2_count);
}

static void
test_wq3_fn(struct work_struct *work)
{
    atomic_inc(&wq3_count);
}

static int
wq_test43_run(void)
{
    struct workqueue_struct *wq1;
    struct workqueue_struct *wq2;
    struct workqueue_struct *wq3;
    struct work_struct work1, work2, work3;
    int errors = 0;

    tbridge_printf("Test: multiple workqueue flush sequence...\n");

    wq1 = alloc_ordered_workqueue("wq1", 0);
    wq2 = alloc_workqueue("wq2", WQ_UNBOUND, 0);
    wq3 = alloc_workqueue("wq3", WQ_HIGHPRI, 0);
    if (wq1 == NULL || wq2 == NULL || wq3 == NULL) {
        tbridge_printf("FAIL: alloc_workqueue() failed\n");
        if (wq1)
            destroy_workqueue(wq1);
        if (wq2)
            destroy_workqueue(wq2);
        if (wq3)
            destroy_workqueue(wq3);
        return 1;
    }

    atomic_set(&wq1_count, 0);
    atomic_set(&wq2_count, 0);
    atomic_set(&wq3_count, 0);

    INIT_WORK(&work1, test_wq1_fn);
    INIT_WORK(&work2, test_wq2_fn);
    INIT_WORK(&work3, test_wq3_fn);

    queue_work(wq1, &work1);
    queue_work(wq2, &work2);
    queue_work(wq3, &work3);

    flush_workqueue(wq1);
    flush_workqueue(wq2);
    flush_workqueue(wq3);

    if (atomic_read(&wq1_count) != 1 || atomic_read(&wq2_count) != 1 ||
        atomic_read(&wq3_count) != 1) {
        tbridge_printf("FAIL: counts wq1=%d wq2=%d wq3=%d\n",
            atomic_read(&wq1_count), atomic_read(&wq2_count),
            atomic_read(&wq3_count));
        errors++;
    } else {
        tbridge_printf("PASS: all workqueues flushed in sequence\n");
    }

    destroy_workqueue(wq1);
    destroy_workqueue(wq2);
    destroy_workqueue(wq3);
    return errors;
}

DEFINE_WQ_TEST(wq_test43, "multiple workqueue flush sequence");
