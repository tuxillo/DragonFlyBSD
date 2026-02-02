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
 * Test 29: cancel_delayed_work_sync() return value (amdgpu ring pattern)
 *
 * This test mirrors the pattern used in amdgpu UVD/VCN ring_begin_use():
 *
 * Real example from amdgpu_uvd.c:
 *   void amdgpu_uvd_ring_begin_use(struct amdgpu_ring *ring) {
 *       set_clocks = !cancel_delayed_work_sync(&adev->uvd.idle_work);
 *       if (set_clocks) {
 *           // Work wasn't pending, need to power up
 *           amdgpu_device_ip_set_powergating_state(..., AMD_PG_STATE_UNGATE);
 *       }
 *   }
 *
 * The return value of cancel_delayed_work_sync() is semantically important:
 * - Returns true if work was pending and successfully cancelled
 * - Returns false if work was not pending (already ran or never queued)
 *
 * This pattern is used for power management race avoidance.
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t work_executed = ATOMIC_INIT(0);
static struct delayed_work test_dwork;

static void
test_work_fn(struct work_struct *work)
{
	atomic_inc(&work_executed);
}

static int
wq_test29_run(void)
{
	int errors = 0;
	bool was_pending;

	tbridge_printf("Test: cancel_delayed_work_sync() return value...\n");

	INIT_DELAYED_WORK(&test_dwork, test_work_fn);

	/*
	 * Test 1: Cancel work that IS pending (should return true)
	 */
	tbridge_printf("  Subtest 1: Cancel pending work...\n");
	atomic_set(&work_executed, 0);

	/* Schedule with long delay so it's definitely still pending */
	schedule_delayed_work(&test_dwork, hz * 10);  /* 10 second delay */

	/* Small delay to ensure it's queued */
	DELAY(1000);  /* 1ms */

	/* Cancel - should return true because work is pending */
	was_pending = cancel_delayed_work_sync(&test_dwork);

	if (was_pending) {
		tbridge_printf("  PASS: cancel returned true for pending work\n");
	} else {
		tbridge_printf("  FAIL: cancel returned false for pending work\n");
		errors++;
	}

	if (atomic_read(&work_executed) == 0) {
		tbridge_printf("  PASS: work was not executed (cancelled in time)\n");
	} else {
		tbridge_printf("  FAIL: work executed despite cancel\n");
		errors++;
	}

	/*
	 * Test 2: Cancel work that is NOT pending (should return false)
	 */
	tbridge_printf("  Subtest 2: Cancel non-pending work...\n");
	atomic_set(&work_executed, 0);

	/* Don't schedule anything, just try to cancel */
	was_pending = cancel_delayed_work_sync(&test_dwork);

	if (!was_pending) {
		tbridge_printf("  PASS: cancel returned false for non-pending work\n");
	} else {
		tbridge_printf("  FAIL: cancel returned true for non-pending work\n");
		errors++;
	}

	/*
	 * Test 3: Cancel work that already executed (should return false)
	 */
	tbridge_printf("  Subtest 3: Cancel already-executed work...\n");
	atomic_set(&work_executed, 0);

	/* Schedule with delay=0 so it runs immediately */
	schedule_delayed_work(&test_dwork, 0);

	/* Wait for execution */
	flush_delayed_work(&test_dwork);

	if (atomic_read(&work_executed) != 1) {
		tbridge_printf("  SKIP: work didn't execute, can't test this case\n");
	} else {
		/* Now try to cancel - should return false since already ran */
		was_pending = cancel_delayed_work_sync(&test_dwork);

		if (!was_pending) {
			tbridge_printf("  PASS: cancel returned false for already-executed work\n");
		} else {
			tbridge_printf("  FAIL: cancel returned true for already-executed work\n");
			errors++;
		}
	}

	/*
	 * Test 4: Simulate amdgpu ring_begin_use pattern
	 */
	tbridge_printf("  Subtest 4: amdgpu ring_begin_use pattern...\n");
	atomic_set(&work_executed, 0);

	/* First, schedule idle work (like ring_end_use does) */
	schedule_delayed_work(&test_dwork, hz / 10);  /* 100ms */

	/* Simulate ring_begin_use: cancel and check return value */
	{
		bool set_clocks = !cancel_delayed_work_sync(&test_dwork);

		if (!set_clocks) {
			/* Work WAS pending - hardware should still be powered */
			tbridge_printf("  PASS: ring_begin_use pattern - work was pending, no clock change needed\n");
		} else {
			/* Work was NOT pending - need to set clocks */
			tbridge_printf("  FAIL: ring_begin_use pattern - unexpected: work wasn't pending\n");
			errors++;
		}
	}

	/* Second call without re-scheduling (simulate double begin_use) */
	{
		bool set_clocks = !cancel_delayed_work_sync(&test_dwork);

		if (set_clocks) {
			/* Work was NOT pending - would need to set clocks */
			tbridge_printf("  PASS: ring_begin_use pattern - second call, work not pending\n");
		} else {
			tbridge_printf("  FAIL: ring_begin_use pattern - second call unexpected pending\n");
			errors++;
		}
	}

	return errors;
}

DEFINE_WQ_TEST(wq_test29, "cancel_delayed_work_sync return value");
