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
 * Test 18: Idle timeout pattern (amdgpu-style power management)
 *
 * Tests a common pattern from GPU drivers:
 * - begin_use(): cancel delayed work, if wasn't pending => power up needed
 * - end_use(): schedule delayed work for idle timeout
 *
 * Pattern:
 * 1. First use cycle - cancel returns false (not pending) => need power up
 * 2. Second use quickly - cancel returns true (still pending) => already powered
 * 3. Third use quickly - cancel returns true => still powered
 * 4. Let idle timeout fire
 * 5. Fourth use - cancel returns false => need power up again
 */

#include "../linuxkpi_workqueue_common.h"

static atomic_t power_up_count = ATOMIC_INIT(0);
static atomic_t idle_timeout_executed = ATOMIC_INIT(0);
static struct delayed_work idle_timeout_dwork;

/* Idle timeout callback - simulates power-down on idle */
static void
test_idle_timeout_fn(struct work_struct *work)
{
	atomic_inc(&idle_timeout_executed);
}

static int
wq_test18_run(void)
{
	int errors = 0;
	bool was_pending;

	tbridge_printf("Test: Idle timeout pattern (amdgpu power management)...\n");

	atomic_set(&power_up_count, 0);
	atomic_set(&idle_timeout_executed, 0);
	INIT_DELAYED_WORK(&idle_timeout_dwork, test_idle_timeout_fn);

	/*
	 * Simulate the amdgpu pattern:
	 * begin_use(): cancel delayed work, if wasn't pending => power up
	 * end_use(): schedule delayed work for idle timeout
	 */

	/* First use cycle - should need to power up */
	was_pending = cancel_delayed_work_sync(&idle_timeout_dwork);
	if (!was_pending) {
		atomic_inc(&power_up_count);
		tbridge_printf("INFO: First use - powered up (cancel returned false)\n");
	}
	/* ... do work ... */
	schedule_delayed_work(&idle_timeout_dwork, hz / 10);  /* 100ms idle timeout */

	/* Second use cycle quickly - should NOT need power up */
	was_pending = cancel_delayed_work_sync(&idle_timeout_dwork);
	if (!was_pending) {
		atomic_inc(&power_up_count);
		tbridge_printf("INFO: Second use - powered up (unexpected)\n");
	} else {
		tbridge_printf("INFO: Second use - still powered (cancel returned true)\n");
	}
	/* ... do work ... */
	schedule_delayed_work(&idle_timeout_dwork, hz / 10);

	/* Third use cycle quickly */
	was_pending = cancel_delayed_work_sync(&idle_timeout_dwork);
	if (!was_pending) {
		atomic_inc(&power_up_count);
	}
	schedule_delayed_work(&idle_timeout_dwork, hz / 10);

	/* Now let idle timeout fire */
	flush_delayed_work(&idle_timeout_dwork);

	/* Fourth use cycle after idle - should need power up again */
	was_pending = cancel_delayed_work_sync(&idle_timeout_dwork);
	if (!was_pending) {
		atomic_inc(&power_up_count);
		tbridge_printf("INFO: Fourth use (after idle) - powered up\n");
	}

	/* Verify pattern worked correctly */
	if (atomic_read(&power_up_count) == 2) {
		tbridge_printf("PASS: Power-up count correct (%d - initial and after idle)\n",
			atomic_read(&power_up_count));
	} else {
		tbridge_printf("FAIL: Expected 2 power-ups, got %d\n",
			atomic_read(&power_up_count));
		errors++;
	}

	if (atomic_read(&idle_timeout_executed) >= 1) {
		tbridge_printf("PASS: Idle timeout executed (%d times)\n",
			atomic_read(&idle_timeout_executed));
	} else {
		tbridge_printf("FAIL: Idle timeout never executed\n");
		errors++;
	}

	/* Cleanup - cancel any pending work */
	cancel_delayed_work_sync(&idle_timeout_dwork);

	return errors;
}

DEFINE_WQ_TEST(wq_test18, "Idle timeout pattern (amdgpu power management)");
