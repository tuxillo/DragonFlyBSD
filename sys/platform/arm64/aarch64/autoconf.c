/*-
 * Copyright (c) 2003-2026 The DragonFly Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * ARM64 autoconfiguration support.
 *
 * Setup the system to run on the current machine. Configure() is called
 * at boot time and initializes the device tables. Available devices are
 * determined and the drivers are initialized.
 *
 * This is a minimal implementation for the ARM64 port MVP. Future work
 * will add FDT and ACPI device enumeration support.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/cons.h>
#include <sys/thread.h>

static void	configure_first(void *);
static void	configure(void *);
static void	configure_final(void *);

SYSINIT(configure1, SI_SUB_CONFIGURE, SI_ORDER_FIRST, configure_first, NULL);
/* SI_ORDER_SECOND is hookable */
SYSINIT(configure2, SI_SUB_CONFIGURE, SI_ORDER_THIRD, configure, NULL);
/* SI_ORDER_MIDDLE is hookable */
SYSINIT(configure3, SI_SUB_CONFIGURE, SI_ORDER_ANY, configure_final, NULL);

cdev_t	rootdev = NULL;
cdev_t	dumpdev = NULL;

/*
 * Determine i/o configuration for a machine.
 */
static void
configure_first(void *dummy)
{
	/* Placeholder for early ARM64 configuration */
}

static void
configure(void *dummy)
{
	/*
	 * This will configure all devices, generally starting with the
	 * nexus (platform/arm64/aarch64/nexus.c). The nexus driver will
	 * enumerate and attach child devices.
	 *
	 * In the future, this will support FDT (Flattened Device Tree)
	 * and ACPI device enumeration.
	 */
	root_bus_configure();

	/*
	 * Allow lowering of the ipl to the lowest kernel level if we
	 * panic (or call tsleep() before clearing `cold').  No level is
	 * completely safe (since a panic may occur in a critical region
	 * at splhigh()), but we want at least bio interrupts to work.
	 */
	safepri = TDPRI_KERN_USER;
}

/*
 * Finalize configure.  Reprobe for the console, in case it was one
 * of the devices which attached, then finish console initialization.
 */
static void
configure_final(void *dummy)
{
	cninit();
	cninit_finish();
}

/*
 * Do legacy root filesystem discovery.
 *
 * For ARM64, this is a placeholder. Future implementations will support:
 * - FDT-based root device discovery
 * - ACPI-based root device discovery
 * - VirtIO block device as root
 */
void
cpu_rootconf(void)
{
	/* Root filesystem discovery - empty for now */
}
SYSINIT(cpu_rootconf, SI_SUB_ROOT_CONF, SI_ORDER_FIRST, cpu_rootconf, NULL);
