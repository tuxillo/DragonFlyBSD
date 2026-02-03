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
 * Test 40: Work queueing from IRQ/softirq context
 */

#include "../linuxkpi_workqueue_common.h"
#include <sys/callout.h>

static atomic_t irq_work_count = ATOMIC_INIT(0);
static struct work_struct irq_work;
static struct callout irq_callout;

static void
test_irq_work_fn(struct work_struct *work)
{
    atomic_inc(&irq_work_count);
}

static void
irq_callout_fn(void *arg)
{
    queue_work(system_unbound_wq, &irq_work);
}

static int
wq_test40_run(void)
{
    int errors = 0;
    int loops = 0;

    tbridge_printf("Test: queueing from IRQ/softirq context...\n");

    atomic_set(&irq_work_count, 0);
    INIT_WORK(&irq_work, test_irq_work_fn);

    callout_init(&irq_callout);
    callout_reset(&irq_callout, hz / 100, irq_callout_fn, NULL);

    while (atomic_read(&irq_work_count) < 1 && loops < 200) {
        pause("wqwait", hz / 100);
        loops++;
    }

    if (atomic_read(&irq_work_count) == 1) {
        tbridge_printf("PASS: work queued from callout executed\n");
    } else {
        tbridge_printf("FAIL: work did not execute (count=%d)\n",
            atomic_read(&irq_work_count));
        errors++;
    }

    callout_stop(&irq_callout);
    flush_work(&irq_work);

    return errors;
}

DEFINE_WQ_TEST(wq_test40, "work queueing from IRQ/softirq context");
