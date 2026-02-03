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
\tatomic_inc(&work_count);
}

static void
test_rcu_fn(struct work_struct *work)
{
\tatomic_inc(&rcu_count);
}

static int
wq_test35_run(void)
{
\tstruct workqueue_struct *wq;
\tint errors = 0;
\tint i;

\ttbridge_printf("Test: iterative drain with RCU barrier...\n");

\twq = alloc_workqueue("drain_rcu_wq", 0, 1);
\tif (wq == NULL) {
\t\ttbridge_printf("FAIL: alloc_workqueue() failed\n");
\t\treturn 1;
\t}

\tatomic_set(&work_count, 0);
\tatomic_set(&rcu_count, 0);

\tfor (i = 0; i < 3; i++) {
\t\tINIT_WORK(&work, test_work_fn);
\t\tINIT_RCU_WORK(&rwork, test_rcu_fn);

\t\tqueue_work(wq, &work);
\t\tqueue_rcu_work(wq, &rwork);

\t\tflush_workqueue(wq);
\t\trcu_barrier();
\t}

\tif (atomic_read(&work_count) == 3) {
\t\ttbridge_printf("PASS: work drained across iterations\n");
\t} else {
\t\ttbridge_printf("FAIL: expected 3 work runs, got %d\n",
\t\t    atomic_read(&work_count));
\t\terrors++;
\t}

\tif (atomic_read(&rcu_count) == 3) {
\t\ttbridge_printf("PASS: rcu work drained across iterations\n");
\t} else {
\t\ttbridge_printf("FAIL: expected 3 rcu runs, got %d\n",
\t\t    atomic_read(&rcu_count));
\t\terrors++;
\t}

\tdestroy_workqueue(wq);
\treturn errors;
}

DEFINE_WQ_TEST(wq_test35, "iterative drain with RCU barrier");
