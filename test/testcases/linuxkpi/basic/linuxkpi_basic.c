/*-
 * Copyright (c) 2026
 *	The DragonFly Project.  All rights reserved.
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

/* kconfig.h is force-included by compiler flags */

#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/tbridge.h>

/* LinuxKPI headers */
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/irq_work.h>
#include <linux/gfp.h>

#include <dfregress.h>

/* Test 1: Memory allocation with LinuxKPI kmalloc */
static int test_memory(void)
{
	void *ptr;
	int errors = 0;

	tbridge_printf("Test 1: LinuxKPI kmalloc()...\n");

	/* Test kmalloc with GFP_KERNEL */
	ptr = kmalloc(1024, GFP_KERNEL);
	if (ptr == NULL) {
		tbridge_printf("FAIL: kmalloc(1024, GFP_KERNEL) returned NULL\n");
		errors++;
	} else {
		tbridge_printf("PASS: kmalloc(1024, GFP_KERNEL) = %p\n", ptr);
		kfree(ptr);
		tbridge_printf("PASS: kfree(%p) completed\n", ptr);
	}

	/* Test kzalloc */
	ptr = kzalloc(512, GFP_KERNEL);
	if (ptr == NULL) {
		tbridge_printf("FAIL: kzalloc(512, GFP_KERNEL) returned NULL\n");
		errors++;
	} else {
		/* Check if zeroed */
		int i, zeroed = 1;
		for (i = 0; i < 512; i++) {
			if (((char *)ptr)[i] != 0) {
				zeroed = 0;
				break;
			}
		}
		if (zeroed) {
			tbridge_printf("PASS: kzalloc returned zeroed memory\n");
		} else {
			tbridge_printf("FAIL: kzalloc did not zero memory\n");
			errors++;
		}
		kfree(ptr);
	}

	/* Test GFP_ATOMIC (should not sleep) */
	ptr = kmalloc(256, GFP_ATOMIC);
	if (ptr == NULL) {
		tbridge_printf("FAIL: kmalloc(256, GFP_ATOMIC) returned NULL\n");
		errors++;
	} else {
		tbridge_printf("PASS: kmalloc(256, GFP_ATOMIC) = %p\n", ptr);
		kfree(ptr);
	}

	return errors;
}

/* Test 2: Workqueue with LinuxKPI */
static int work_counter = 0;

static void test_work_fn(struct work_struct *work)
{
	work_counter++;
}

static int test_workqueue(void)
{
	struct work_struct work;
	struct delayed_work dwork;
	int errors = 0;

	tbridge_printf("\nTest 2: LinuxKPI workqueue...\n");

	/* Test basic workqueue */
	work_counter = 0;
	INIT_WORK(&work, test_work_fn);
	
	if (!queue_work(system_wq, &work)) {
		tbridge_printf("FAIL: queue_work() failed\n");
		errors++;
	} else {
		tbridge_printf("PASS: queue_work() succeeded\n");
		flush_work(&work);
		if (work_counter == 1) {
			tbridge_printf("PASS: work callback executed (count=%d)\n", work_counter);
		} else {
			tbridge_printf("FAIL: work callback not executed (count=%d)\n", work_counter);
			errors++;
		}
	}

	/* Test delayed work */
	work_counter = 0;
	INIT_DELAYED_WORK(&dwork, test_work_fn);
	
	if (!schedule_delayed_work(&dwork, 1)) {
		tbridge_printf("FAIL: schedule_delayed_work() failed\n");
		errors++;
	} else {
		tbridge_printf("PASS: schedule_delayed_work() succeeded\n");
		flush_delayed_work(&dwork);
		if (work_counter == 1) {
			tbridge_printf("PASS: delayed work callback executed\n");
		} else {
			tbridge_printf("FAIL: delayed work callback not executed\n");
			errors++;
		}
	}

	return errors;
}

/* Test 3: IRQ work with LinuxKPI */
static int irqwork_counter = 0;

static void test_irqwork_fn(struct irq_work *work)
{
	irqwork_counter++;
}

static int test_irqwork(void)
{
	struct irq_work irqwork;
	int errors = 0;

	tbridge_printf("\nTest 3: LinuxKPI irq_work...\n");

	irqwork_counter = 0;
	init_irq_work(&irqwork, test_irqwork_fn);

	if (!irq_work_queue(&irqwork)) {
		tbridge_printf("INFO: irq_work_queue() returned false (may be already pending)\n");
	} else {
		tbridge_printf("PASS: irq_work_queue() succeeded\n");
	}

	irq_work_sync(&irqwork);

	if (irqwork_counter == 1) {
		tbridge_printf("PASS: irq_work callback executed (count=%d)\n", irqwork_counter);
	} else {
		tbridge_printf("FAIL: irq_work callback not executed (count=%d)\n", irqwork_counter);
		errors++;
	}

	return errors;
}

/* Main test runner */
static void
linuxkpi_basic_run(void *arg __unused)
{
	int total_errors = 0;

	tbridge_printf("========================================\n");
	tbridge_printf("LinuxKPI Functional Test (dfregress)\n");
	tbridge_printf("Actually exercising LinuxKPI APIs!\n");
	tbridge_printf("========================================\n\n");

	/* Test 1: Memory */
	total_errors += test_memory();

	/* Test 2: Workqueue */
	total_errors += test_workqueue();

	/* Test 3: IRQ Work */
	total_errors += test_irqwork();

	tbridge_printf("\n========================================\n");
	if (total_errors == 0) {
		tbridge_printf("ALL LinuxKPI TESTS PASSED!\n");
		tbridge_printf("========================================\n");
		tbridge_test_done(RESULT_PASS);
	} else {
		tbridge_printf("TESTS FAILED: %d errors\n", total_errors);
		tbridge_printf("========================================\n");
		tbridge_test_done(RESULT_FAIL);
	}
}

static struct tbridge_testcase linuxkpi_basic_case = {
	.tb_run		= linuxkpi_basic_run,
	.tb_abort	= NULL
};

TBRIDGE_TESTCASE_MODULE(linuxkpi_basic, &linuxkpi_basic_case);
