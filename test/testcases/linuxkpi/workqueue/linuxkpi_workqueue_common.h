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

#ifndef _LINUXKPI_WORKQUEUE_COMMON_H_
#define _LINUXKPI_WORKQUEUE_COMMON_H_

/*
 * Common header for LinuxKPI workqueue tests.
 * Each test module includes this for shared utilities.
 */

/* kconfig.h is force-included by compiler flags */

#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/tbridge.h>
#include <sys/time.h>

/* LinuxKPI headers */
#include <linux/workqueue.h>
#include <linux/gfp.h>
#include <linux/atomic.h>

#include <dfregress.h>

/*
 * Helper to compute elapsed time in milliseconds.
 */
static inline long
wq_elapsed_ms(struct timeval *start, struct timeval *end)
{
	return (end->tv_sec - start->tv_sec) * 1000 +
	       (end->tv_usec - start->tv_usec) / 1000;
}

/*
 * Macro to define a simple workqueue test module.
 * Usage:
 *   static int my_test_run(void) { ... return errors; }
 *   DEFINE_WQ_TEST(my_test, "Test description");
 */
#define DEFINE_WQ_TEST(name, desc)					\
									\
static void								\
name##_run_wrapper(void *arg __unused)					\
{									\
	int errors;							\
	struct timeval start, end;					\
									\
	microtime(&start);						\
	tbridge_printf("========================================\n");	\
	tbridge_printf("LinuxKPI Workqueue Test: %s\n", desc);		\
	tbridge_printf("========================================\n\n");	\
									\
	errors = name##_run();						\
									\
	microtime(&end);						\
	tbridge_printf("\n========================================\n");	\
	tbridge_printf("Time: %ld ms\n", wq_elapsed_ms(&start, &end));	\
	if (errors == 0) {						\
		tbridge_printf("TEST PASSED\n");			\
		tbridge_printf("========================================\n"); \
		tbridge_test_done(RESULT_PASS);				\
	} else {							\
		tbridge_printf("TEST FAILED: %d errors\n", errors);	\
		tbridge_printf("========================================\n"); \
		tbridge_test_done(RESULT_FAIL);				\
	}								\
}									\
									\
static struct tbridge_testcase name##_testcase = {			\
	.tb_run		= name##_run_wrapper,				\
	.tb_abort	= NULL						\
};									\
									\
TBRIDGE_TESTCASE_MODULE(name, &name##_testcase)

#endif /* _LINUXKPI_WORKQUEUE_COMMON_H_ */
