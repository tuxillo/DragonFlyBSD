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
 * Test 44: Reference-counted work
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t refcount = ATOMIC_INIT(1);
static struct work_struct ref_work;

static void
test_ref_fn(struct work_struct *work)
{
\tatomic_dec(&refcount);
}

static int
wq_test44_run(void)
{
\tint errors = 0;

\ttbridge_printf("Test: reference-counted work...\n");

\tINIT_WORK(&ref_work, test_ref_fn);

\tatomic_inc(&refcount);
\tqueue_work(system_wq, &ref_work);
\tflush_work(&ref_work);

\tif (atomic_read(&refcount) == 1) {
\t\ttbridge_printf("PASS: refcount released by work\n");
\t} else {
\t\ttbridge_printf("FAIL: expected refcount 1, got %d\n",
\t\t    atomic_read(&refcount));
\t\terrors++;
\t}

\tatomic_dec(&refcount);
\tif (atomic_read(&refcount) != 0) {
\t\ttbridge_printf("FAIL: expected refcount 0 after final put\n");
\t\terrors++;
\t}

\treturn errors;
}

DEFINE_WQ_TEST(wq_test44, "reference-counted work");
