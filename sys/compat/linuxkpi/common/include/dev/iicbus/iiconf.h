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

#ifndef _LINUXKPI_DEV_IICBUS_IICONF_H_
#define _LINUXKPI_DEV_IICBUS_IICONF_H_

/*
 * LinuxKPI I2C configuration compatibility for DragonFly BSD.
 *
 * This wraps DragonFly's native I2C subsystem (bus/iicbus/) for
 * compatibility with FreeBSD/LinuxKPI code.
 */

#ifdef __DragonFly__

/*
 * Include DragonFly's native IIC definitions.
 * These provide IIC_E* error codes, iicbus_transfer, etc.
 */
#include <bus/iicbus/iiconf.h>

/*
 * iicbus_transfer_gen - FreeBSD generic I2C transfer function.
 * DragonFly: Use the bus-layer transfer methods.
 */
#ifndef iicbus_transfer_gen
#define iicbus_transfer_gen(bus, msgs, nmsgs) \
	IICBUS_TRANSFER(bus, msgs, nmsgs)
#endif

#else /* FreeBSD */

/* On FreeBSD, include the native header */
#include_next <dev/iicbus/iiconf.h>

#endif /* __DragonFly__ */

#endif /* _LINUXKPI_DEV_IICBUS_IICONF_H_ */
