/*-
 * Copyright (c) 2026 The DragonFly Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _BACKLIGHT_IF_H_
#define _BACKLIGHT_IF_H_

/*
 * DragonFly backlight interface for LinuxKPI.
 *
 * This provides the BSD-style backlight device interface methods.
 * Note: This file defines BSD-side functions, not Linux-side.
 * The Linux-side backlight functions are in linux/backlight.h.
 *
 * These functions use _desc suffix to avoid conflicts with the
 * LinuxKPI inline functions in linux/backlight.h.
 */

#include <sys/types.h>
#include <sys/bus.h>

/*
 * FreeBSD generates backlight_if.h from backlight_if.m with kobj.
 * DragonFly doesn't have this interface. We need to provide:
 * 1. Method descriptor declarations (for DEVMETHOD)
 * 2. Stub implementations for the interface
 */

#ifdef __DragonFly__

/*
 * Method descriptors - DragonFly doesn't generate these.
 * Declare as extern to satisfy DEVMETHOD references, but they
 * won't actually exist at link time unless we provide them.
 *
 * For now, disable the backlight DEVMETHOD entries in linux_pci.c
 * since DragonFly doesn't have this device interface.
 */

/* Stub - backlight interface not supported on DragonFly */
#define BACKLIGHT_IF_STUB 1

#else /* FreeBSD */

/* FreeBSD would include the generated backlight_if.h here */

#endif /* __DragonFly__ */

#endif /* _BACKLIGHT_IF_H_ */
