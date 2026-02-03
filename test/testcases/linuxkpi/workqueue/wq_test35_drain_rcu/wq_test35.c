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
 * Test 35: Iterative drain with RCU barrier
 */

#include "../linuxkpi_workqueue_common.h"
#include <linux/rcupdate.h>

static atomic_t work_count = ATOMIC_INIT(0);
static atomic_t rcu_count = ATOMIC_INIT(0);
static struct rcu_work rwork;
static struct work_struct work;

static void
test_work_fn(struct work_struct *work)
{
    atomic_inc(&work_count);
}

static void
test_rcu_fn(struct work_struct *work)
{
    atomic_inc(&rcu_count);
}

static int
wq_test35_run(void)
{
    struct workqueue_struct *wq;
    int errors = 0;
    int i;

    tbridge_printf("Test: iterative drain with RCU barrier...\n");

    wq = alloc_workqueue("drain_rcu_wq", 0, 1);
    if (wq == NULL) {
        tbridge_printf("FAIL: alloc_workqueue() failed\n");
        return 1;
    }

    atomic_set(&work_count, 0);
    atomic_set(&rcu_count, 0);

    for (i = 0; i < 3; i++) {
        INIT_WORK(&work, test_work_fn);
        INIT_RCU_WORK(&rwork, test_rcu_fn);

        queue_work(wq, &work);
        queue_rcu_work(wq, &rwork);

        flush_workqueue(wq);
        rcu_barrier();
    }

    if (atomic_read(&work_count) == 3) {
        tbridge_printf("PASS: work drained across iterations\n");
    } else {
        tbridge_printf("FAIL: expected 3 work runs, got %d\n",
            atomic_read(&work_count));
        errors++;
    }

    if (atomic_read(&rcu_count) == 3) {
        tbridge_printf("PASS: rcu work drained across iterations\n");
    } else {
        tbridge_printf("FAIL: expected 3 rcu runs, got %d\n",
            atomic_read(&rcu_count));
        errors++;
    }

    destroy_workqueue(wq);
    return errors;
}

DEFINE_WQ_TEST(wq_test35, "iterative drain with RCU barrier");
