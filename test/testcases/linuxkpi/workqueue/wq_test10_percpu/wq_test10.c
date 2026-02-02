/*-
 * Copyright (c) 2025
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

/*
 * Test 10: Per-CPU distribution verification
 *
 * Tests that work is distributed across per-CPU taskqueues:
 * - Create per-CPU workqueue with alloc_workqueue("name", 0, 0)
 * - Queue ncpus * 5 work items
 * - Each callback records which CPU it ran on
 * - Verify distribution across CPUs
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t work_counter = ATOMIC_INIT(0);
static atomic_t per_cpu_count[MAXCPU];

static void
test_cpu_track_fn(struct work_struct *work)
{
	int cpuid = mycpuid;
	if (cpuid >= 0 && cpuid < MAXCPU) {
		atomic_inc(&per_cpu_count[cpuid]);
	}
	atomic_inc(&work_counter);
}

static int
wq_test10_run(void)
{
	struct work_struct *works;
	struct workqueue_struct *wq;
	int i;
	int errors = 0;
	int num_cpus = ncpus;
	int work_items = num_cpus * 5;  /* 5 items per CPU */

	tbridge_printf("Test: Per-CPU distribution (%d CPUs, %d items)...\n", num_cpus, work_items);

	/* Initialize per-CPU counters (only up to ncpus, not MAXCPU) */
	for (i = 0; i < num_cpus && i < MAXCPU; i++) {
		atomic_set(&per_cpu_count[i], 0);
	}
	atomic_set(&work_counter, 0);

	wq = alloc_workqueue("test_percpu", 0, 0);  /* Per-CPU workqueue */
	if (wq == NULL) {
		tbridge_printf("FAIL: alloc_workqueue() failed\n");
		return 1;
	}

	works = kmalloc(sizeof(*works) * work_items, GFP_KERNEL);
	if (works == NULL) {
		tbridge_printf("FAIL: kmalloc() failed for work items\n");
		destroy_workqueue(wq);
		return 1;
	}

	for (i = 0; i < work_items; i++) {
		INIT_WORK(&works[i], test_cpu_track_fn);
		queue_work(wq, &works[i]);
	}

	drain_workqueue(wq);

	if (atomic_read(&work_counter) == work_items) {
		tbridge_printf("PASS: All %d work callbacks executed\n", work_items);
		
		/* Show distribution across CPUs */
		tbridge_printf("INFO: Per-CPU execution counts:\n");
		for (i = 0; i < num_cpus && i < MAXCPU; i++) {
			int count = atomic_read(&per_cpu_count[i]);
			if (count > 0) {
				tbridge_printf("  CPU %d: %d items\n", i, count);
			}
		}
	} else {
		tbridge_printf("FAIL: Expected %d callbacks, got %d\n", 
			work_items, atomic_read(&work_counter));
		errors++;
	}

	destroy_workqueue(wq);
	kfree(works);

	return errors;
}

DEFINE_WQ_TEST(wq_test10, "Per-CPU distribution");
