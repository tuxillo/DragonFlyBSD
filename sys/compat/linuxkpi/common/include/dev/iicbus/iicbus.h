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

#ifndef _LINUXKPI_DEV_IICBUS_IICBUS_H_
#define _LINUXKPI_DEV_IICBUS_IICBUS_H_

/*
 * LinuxKPI I2C bus compatibility layer for DragonFly BSD.
 *
 * This wraps DragonFly's native IIC subsystem for FreeBSD/LinuxKPI
 * compatibility. We include the native DragonFly headers which provide
 * struct iic_msg, IIC_M_* flags, etc.
 */

#include <sys/types.h>
#include <sys/bus.h>

#ifdef __DragonFly__
/*
 * Include DragonFly's native I2C headers.
 * bus/iicbus/iic.h provides: struct iic_msg, IIC_M_* flags
 * bus/iicbus/iiconf.h provides: IIC_E* error codes
 * bus/iicbus/iicbus.h provides: IICBUS_* ivar definitions
 */
#include <bus/iicbus/iic.h>
#include <bus/iicbus/iicbus.h>
#endif

/*
 * Additional flags that may not be in DragonFly's native headers.
 * Define only if not already defined by the native headers.
 */
#ifndef IIC_M_WR
#define IIC_M_WR	0x0000
#endif
#ifndef IIC_M_RD
#define IIC_M_RD	0x0001
#endif
#ifndef IIC_M_NOSTOP
#define IIC_M_NOSTOP	0x0002
#endif
#ifndef IIC_M_NOSTART
#define IIC_M_NOSTART	0x0004
#endif

/* I2C bus error codes - only define if not already defined */
#ifndef IICBUS_NOERR
#define IICBUS_NOERR	0
#endif
#ifndef IICBUS_ERROR
#define IICBUS_ERROR	1
#endif

#endif /* _LINUXKPI_DEV_IICBUS_IICBUS_H_ */
