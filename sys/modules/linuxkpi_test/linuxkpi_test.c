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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/sysctl.h>

/* LinuxKPI headers */
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/irq_work.h>
#include <linux/timer.h>
#include <linux/idr.h>
#include <linux/gfp.h>

/* Test module name and version */
#define LKPI_TEST_VERSION "1.0"

/* Test results structure */
struct lkpi_test_results {
	int memory_passed;
	int workqueue_passed;
	int irqwork_passed;
	int slab_passed;
	int timer_passed;
	int idr_passed;
	char error_msg[256];
};

static struct lkpi_test_results test_results;
static int test_running = 0;

/*
 * Test 1: Memory allocation
 * Tests kmalloc, kzalloc, kfree with various sizes and GFP flags
 */
static int
lkpi_test_memory(void)
{
	void *ptr1, *ptr2, *ptr3;
	int errors = 0;

	printk("LKPI Test: Memory allocation starting...\n");

	/* Test 1a: Basic kmalloc with GFP_KERNEL */
	ptr1 = kmalloc(1024, GFP_KERNEL);
	if (ptr1 == NULL) {
		printk("LKPI Test ERROR: kmalloc(1024, GFP_KERNEL) failed\n");
		errors++;
	} else {
		kfree(ptr1);
		printk("LKPI Test: kmalloc(1024, GFP_KERNEL) passed\n");
	}

	/* Test 1b: kzalloc - should return zeroed memory */
	ptr2 = kzalloc(512, GFP_KERNEL);
	if (ptr2 == NULL) {
		printk("LKPI Test ERROR: kzalloc(512, GFP_KERNEL) failed\n");
		errors++;
	} else {
		/* Check if memory is zeroed */
		int i, zeroed = 1;
		for (i = 0; i < 512; i++) {
			if (((char *)ptr2)[i] != 0) {
				zeroed = 0;
				break;
			}
		}
		if (zeroed) {
			printk("LKPI Test: kzalloc(512, GFP_KERNEL) passed (zeroed)\n");
		} else {
			printk("LKPI Test ERROR: kzalloc did not zero memory\n");
			errors++;
		}
		kfree(ptr2);
	}

	/* Test 1c: GFP_ATOMIC allocation (should not sleep) */
	ptr3 = kmalloc(256, GFP_ATOMIC);
	if (ptr3 == NULL) {
		printk("LKPI Test ERROR: kmalloc(256, GFP_ATOMIC) failed\n");
		errors++;
	} else {
		kfree(ptr3);
		printk("LKPI Test: kmalloc(256, GFP_ATOMIC) passed\n");
	}

	/* Test 1d: Various sizes */
	int sizes[] = {1, 8, 64, 256, 1024, 4096, 8192};
	int i;
	for (i = 0; i < 7; i++) {
		void *p = kmalloc(sizes[i], GFP_KERNEL);
		if (p == NULL) {
			printk("LKPI Test ERROR: kmalloc(%d) failed\n", sizes[i]);
			errors++;
		} else {
			kfree(p);
		}
	}
	if (errors == 0) {
		printk("LKPI Test: Memory allocation - ALL PASSED\n");
	}

	return errors;
}

/*
 * Test 2: Workqueue
 * Tests queue_work, schedule_delayed_work, flush_work
 */
static int workqueue_test_count = 0;

static void
workqueue_test_fn(struct work_struct *work)
{
	workqueue_test_count++;
	printk("LKPI Test: Workqueue callback executed (count=%d)\n", 
	    workqueue_test_count);
}

static int
lkpi_test_workqueue(void)
{
	struct work_struct test_work;
	struct delayed_work test_dwork;
	int errors = 0;

	printk("LKPI Test: Workqueue starting...\n");

	/* Test 2a: Basic queue_work on system_wq */
	workqueue_test_count = 0;
	INIT_WORK(&test_work, workqueue_test_fn);
	
	if (!queue_work(system_wq, &test_work)) {
		printk("LKPI Test ERROR: queue_work failed\n");
		errors++;
	} else {
		/* Flush and wait for completion */
		flush_work(&test_work);
		if (workqueue_test_count == 1) {
			printk("LKPI Test: queue_work passed\n");
		} else {
			printk("LKPI Test ERROR: work callback not executed\n");
			errors++;
		}
	}

	/* Test 2b: Delayed work */
	workqueue_test_count = 0;
	INIT_DELAYED_WORK(&test_dwork, workqueue_test_fn);
	
	if (!schedule_delayed_work(&test_dwork, 1)) { /* 1 tick delay */
		printk("LKPI Test ERROR: schedule_delayed_work failed\n");
		errors++;
	} else {
		/* Flush delayed work */
		flush_delayed_work(&test_dwork);
		if (workqueue_test_count == 1) {
			printk("LKPI Test: schedule_delayed_work passed\n");
		} else {
			printk("LKPI Test ERROR: delayed work callback not executed\n");
			errors++;
		}
	}

	/* Test 2c: Multiple work items */
	workqueue_test_count = 0;
	struct work_struct works[5];
	int i;
	for (i = 0; i < 5; i++) {
		INIT_WORK(&works[i], workqueue_test_fn);
		queue_work(system_wq, &works[i]);
	}
	/* Flush all */
	for (i = 0; i < 5; i++) {
		flush_work(&works[i]);
	}
	if (workqueue_test_count == 5) {
		printk("LKPI Test: Multiple work items passed\n");
	} else {
		printk("LKPI Test ERROR: Expected 5 callbacks, got %d\n", workqueue_test_count);
		errors++;
	}

	if (errors == 0) {
		printk("LKPI Test: Workqueue - ALL PASSED\n");
	}

	return errors;
}

/*
 * Test 3: IRQ Work
 * Tests irq_work_queue, irq_work_sync
 */
static int irqwork_test_count = 0;

static void
irqwork_test_fn(struct irq_work *work)
{
	irqwork_test_count++;
	printk("LKPI Test: IRQ work callback executed (count=%d)\n", 
	    irqwork_test_count);
}

static int
lkpi_test_irqwork(void)
{
	struct irq_work test_irqwork;
	int errors = 0;

	printk("LKPI Test: IRQ Work starting...\n");

	/* Initialize IRQ work */
	init_irq_work(&test_irqwork, irqwork_test_fn);

	/* Test 3a: Queue from process context */
	irqwork_test_count = 0;
	if (!irq_work_queue(&test_irqwork)) {
		printk("LKPI Test: irq_work_queue returned false (already pending or error)\n");
	}

	/* Sync and wait */
	irq_work_sync(&test_irqwork);
	
	if (irqwork_test_count == 1) {
		printk("LKPI Test: irq_work_queue and irq_work_sync passed\n");
	} else {
		printk("LKPI Test ERROR: IRQ work callback not executed (count=%d)\n", 
		    irqwork_test_count);
		errors++;
	}

	/* Test 3b: Re-queue after completion */
	irqwork_test_count = 0;
	if (!irq_work_queue(&test_irqwork)) {
		printk("LKPI Test ERROR: irq_work_queue failed on second attempt\n");
		errors++;
	} else {
		irq_work_sync(&test_irqwork);
		if (irqwork_test_count == 1) {
			printk("LKPI Test: Re-queue after completion passed\n");
		} else {
			printk("LKPI Test ERROR: Re-queue callback not executed\n");
			errors++;
		}
	}

	if (errors == 0) {
		printk("LKPI Test: IRQ Work - ALL PASSED\n");
	}

	return errors;
}

/*
 * Test 4: Slab cache
 * Tests kmem_cache_create, kmem_cache_alloc, kmem_cache_free
 */
static int slab_test_count = 0;

static void
slab_ctor(void *obj)
{
	slab_test_count++;
}

static int
lkpi_test_slab(void)
{
	struct kmem_cache *cache;
	void *obj1, *obj2;
	int errors = 0;

	printk("LKPI Test: Slab cache starting...\n");

	/* Test 4a: Create cache */
	cache = kmem_cache_create("test_cache", 128, 0, 0, slab_ctor);
	if (cache == NULL) {
		printk("LKPI Test ERROR: kmem_cache_create failed\n");
		return 1;
	}
	printk("LKPI Test: kmem_cache_create passed\n");

	/* Test 4b: Allocate objects */
	slab_test_count = 0;
	obj1 = kmem_cache_alloc(cache, GFP_KERNEL);
	if (obj1 == NULL) {
		printk("LKPI Test ERROR: kmem_cache_alloc failed\n");
		errors++;
	} else {
		printk("LKPI Test: kmem_cache_alloc passed (ctor called %d times)\n", 
		    slab_test_count);
	}

	obj2 = kmem_cache_alloc(cache, GFP_KERNEL);
	if (obj2 == NULL) {
		printk("LKPI Test ERROR: kmem_cache_alloc (2nd) failed\n");
		errors++;
	}

	/* Test 4c: Free objects */
	if (obj1) {
		kmem_cache_free(cache, obj1);
		printk("LKPI Test: kmem_cache_free passed\n");
	}
	if (obj2) {
		kmem_cache_free(cache, obj2);
	}

	/* Test 4d: Destroy cache */
	kmem_cache_destroy(cache);
	printk("LKPI Test: kmem_cache_destroy passed\n");

	if (errors == 0) {
		printk("LKPI Test: Slab cache - ALL PASSED\n");
	}

	return errors;
}

/*
 * Test 5: IDR (ID Allocation)
 * Tests idr_alloc, idr_remove, idr_find
 */
static int
lkpi_test_idr(void)
{
	struct idr idr;
	int id1, id2, id3;
	void *ptr;
	int errors = 0;

	printk("LKPI Test: IDR starting...\n");

	/* Initialize IDR */
	idr_init(&idr);
	printk("LKPI Test: idr_init passed\n");

	/* Test 5a: Allocate IDs */
	id1 = idr_alloc(&idr, (void *)0x1234, 0, 0, GFP_KERNEL);
	if (id1 < 0) {
		printk("LKPI Test ERROR: idr_alloc failed (ret=%d)\n", id1);
		errors++;
	} else {
		printk("LKPI Test: idr_alloc passed (id=%d)\n", id1);
	}

	id2 = idr_alloc(&idr, (void *)0x5678, 0, 0, GFP_KERNEL);
	if (id2 < 0) {
		printk("LKPI Test ERROR: idr_alloc (2nd) failed (ret=%d)\n", id2);
		errors++;
	} else {
		printk("LKPI Test: idr_alloc (2nd) passed (id=%d)\n", id2);
	}

	/* Test 5b: Find IDs */
	if (id1 >= 0) {
		ptr = idr_find(&idr, id1);
		if (ptr == (void *)0x1234) {
			printk("LKPI Test: idr_find passed\n");
		} else {
			printk("LKPI Test ERROR: idr_find returned wrong value\n");
			errors++;
		}
	}

	/* Test 5c: Remove IDs */
	if (id1 >= 0) {
		idr_remove(&idr, id1);
		printk("LKPI Test: idr_remove passed\n");
	}
	if (id2 >= 0) {
		idr_remove(&idr, id2);
	}

	/* Test 5d: Allocate with specific range */
	id3 = idr_alloc(&idr, (void *)0x9999, 100, 200, GFP_KERNEL);
	if (id3 < 0 || id3 < 100 || id3 >= 200) {
		printk("LKPI Test ERROR: idr_alloc with range failed\n");
		errors++;
	} else {
		printk("LKPI Test: idr_alloc with range passed (id=%d)\n", id3);
		idr_remove(&idr, id3);
	}

	/* Destroy IDR */
	idr_destroy(&idr);
	printk("LKPI Test: idr_destroy passed\n");

	if (errors == 0) {
		printk("LKPI Test: IDR - ALL PASSED\n");
	}

	return errors;
}

/*
 * Run all tests
 */
static int
lkpi_run_all_tests(void)
{
	int total_errors = 0;

	if (test_running) {
		printk("LKPI Test: Tests already running\n");
		return -1;
	}

	test_running = 1;
	memset(&test_results, 0, sizeof(test_results));

	printk("\n========== LinuxKPI Functional Tests Starting ==========\n");

	/* Test 1: Memory */
	test_results.memory_passed = (lkpi_test_memory() == 0);
	total_errors += test_results.memory_passed ? 0 : 1;

	/* Test 2: Workqueue */
	test_results.workqueue_passed = (lkpi_test_workqueue() == 0);
	total_errors += test_results.workqueue_passed ? 0 : 1;

	/* Test 3: IRQ Work */
	test_results.irqwork_passed = (lkpi_test_irqwork() == 0);
	total_errors += test_results.irqwork_passed ? 0 : 1;

	/* Test 4: Slab cache */
	test_results.slab_passed = (lkpi_test_slab() == 0);
	total_errors += test_results.slab_passed ? 0 : 1;

	/* Test 5: IDR */
	test_results.idr_passed = (lkpi_test_idr() == 0);
	total_errors += test_results.idr_passed ? 0 : 1;

	printk("\n========== LinuxKPI Functional Tests Complete ==========\n");
	printk("Results:\n");
	printk("  Memory:     %s\n", test_results.memory_passed ? "PASS" : "FAIL");
	printk("  Workqueue:  %s\n", test_results.workqueue_passed ? "PASS" : "FAIL");
	printk("  IRQ Work:   %s\n", test_results.irqwork_passed ? "PASS" : "FAIL");
	printk("  Slab:       %s\n", test_results.slab_passed ? "PASS" : "FAIL");
	printk("  IDR:        %s\n", test_results.idr_passed ? "PASS" : "FAIL");
	printk("Total: %d tests passed, %d failed\n",
	    5 - total_errors, total_errors);
	printk("=======================================================\n\n");

	test_running = 0;
	return total_errors;
}

/*
 * Sysctl interface
 */
static int
sysctl_lkpi_test_run(SYSCTL_HANDLER_ARGS)
{
	int error, val = 0;

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || req->newptr == NULL)
		return error;

	if (val == 1) {
		lkpi_run_all_tests();
	}

	return 0;
}

static int
sysctl_lkpi_test_results(SYSCTL_HANDLER_ARGS)
{
	char buf[512];
	snprintf(buf, sizeof(buf),
	    "Memory: %s, Workqueue: %s, IRQ: %s, Slab: %s, IDR: %s",
	    test_results.memory_passed ? "PASS" : "FAIL",
	    test_results.workqueue_passed ? "PASS" : "FAIL",
	    test_results.irqwork_passed ? "PASS" : "FAIL",
	    test_results.slab_passed ? "PASS" : "FAIL",
	    test_results.idr_passed ? "PASS" : "FAIL");
	return sysctl_handle_string(oidp, buf, 0, req);
}

SYSCTL_NODE(_debug, OID_AUTO, lkpi_test, CTLFLAG_RW, 0, "LinuxKPI Test");
SYSCTL_PROC(_debug_lkpi_test, OID_AUTO, run, CTLTYPE_INT | CTLFLAG_RW,
	0, 0, sysctl_lkpi_test_run, "I", "Run LinuxKPI tests (set to 1)");
SYSCTL_PROC(_debug_lkpi_test, OID_AUTO, results, CTLTYPE_STRING | CTLFLAG_RD,
	0, 0, sysctl_lkpi_test_results, "A", "Last test results");
SYSCTL_INT(_debug_lkpi_test, OID_AUTO, running, CTLFLAG_RD,
	&test_running, 0, "Tests currently running");

/*
 * Module load/unload
 */
static int
lkpi_test_module_init(void)
{
	printk("LinuxKPI Test Module v%s loaded\n", LKPI_TEST_VERSION);
	printk("Set debug.lkpi_test.run=1 to execute tests\n");
	return 0;
}

static int
lkpi_test_module_exit(void)
{
	printk("LinuxKPI Test Module unloaded\n");
	return 0;
}

/* Run tests automatically on load for now */
static void
lkpi_test_auto_run(void *arg __unused)
{
	/* Delay to let system settle */
	pause("lkitest", hz * 2);
	lkpi_run_all_tests();
}

static int
lkpi_test_event_handler(module_t mod, int event, void *arg __unused)
{
	int error = 0;

	switch (event) {
	case MOD_LOAD:
		error = lkpi_test_module_init();
		if (error == 0) {
			/* Schedule auto-run after boot settles */
			taskqueue_enqueue_timeout(taskqueue_thread, 
			    NULL, lkpi_test_auto_run, NULL);
		}
		break;
	case MOD_UNLOAD:
		error = lkpi_test_module_exit();
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return error;
}

DEV_MODULE(lkpi_test, lkpi_test_event_handler, NULL);
MODULE_VERSION(lkpi_test, 1);
MODULE_DEPEND(lkpi_test, linuxkpi, 1, 1, 1);
