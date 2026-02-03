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
 * Test 38: WQ_HIGHPRI | WQ_UNBOUND combined flags
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t work_count = ATOMIC_INIT(0);

static void
test_work_fn(struct work_struct *work)
{
\tatomic_inc(&work_count);
}

static int
wq_test38_run(void)
{
\tstruct workqueue_struct *wq;
\tstruct work_struct work;
\tint errors = 0;

\ttbridge_printf("Test: WQ_HIGHPRI | WQ_UNBOUND combined...\n");

\twq = alloc_workqueue("hp_unbound", WQ_HIGHPRI | WQ_UNBOUND, 0);
\tif (wq == NULL) {
\t\ttbridge_printf("FAIL: alloc_workqueue() failed\n");
\t\treturn 1;
\t}

\tatomic_set(&work_count, 0);
\tINIT_WORK(&work, test_work_fn);
\tqueue_work(wq, &work);
\tflush_work(&work);

\tif (atomic_read(&work_count) == 1) {
\t\ttbridge_printf("PASS: combined flags work executed\n");
\t} else {
\t\ttbridge_printf("FAIL: expected 1 execution, got %d\n",
\t\t    atomic_read(&work_count));
\t\terrors++;
\t}

\tdestroy_workqueue(wq);
\treturn errors;
}

DEFINE_WQ_TEST(wq_test38, "WQ_HIGHPRI | WQ_UNBOUND combined");
