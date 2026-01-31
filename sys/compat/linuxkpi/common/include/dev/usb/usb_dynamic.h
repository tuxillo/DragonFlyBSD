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

#ifndef _USB_DYNAMIC_H_
#define _USB_DYNAMIC_H_

/* DragonFly USB dynamic descriptors for LinuxKPI */

#include <sys/types.h>
#include <dev/usb/usb.h>

/*
 * USB dynamic descriptors allow descriptors to be created and modified
 * at runtime, which is useful for composite devices and dynamic
 * configuration changes.
 */

/* USB dynamic descriptor types */
#define USB_DYNAMIC_DESC_DEVICE			0x01
#define USB_DYNAMIC_DESC_CONFIG			0x02
#define USB_DYNAMIC_DESC_STRING			0x03
#define USB_DYNAMIC_DESC_INTERFACE		0x04
#define USB_DYNAMIC_DESC_ENDPOINT		0x05
#define USB_DYNAMIC_DESC_FULL_CONFIG		0x06
#define USB_DYNAMIC_DESC_OTHER_SPEED		0x07

/* USB dynamic descriptor flags */
#define USB_DYNAMIC_FLAG_ALLOCATED		0x0001
#define USB_DYNAMIC_FLAG_FIXED			0x0002
#define USB_DYNAMIC_FLAG_IMMUTABLE		0x0004
#define USB_DYNAMIC_FLAG_ZCOPY			0x0008

/* USB dynamic descriptor limits */
#define USB_DYNAMIC_MAX_DESCRIPTORS		32
#define USB_DYNAMIC_MAX_CONFIGS			8
#define USB_DYNAMIC_MAX_INTERFACES		16
#define USB_DYNAMIC_MAX_ENDPOINTS		32
#define USB_DYNAMIC_MAX_STRINGS			64
#define USB_DYNAMIC_MAX_BUFFER			4096

/* USB dynamic descriptor structure */
struct usb_dynamic_desc {
	uint8_t			type;
	uint8_t			subtype;
	uint16_t		flags;
	uint16_t		index;
	uint16_t		length;
	void			*data;
	struct usb_dynamic_desc	*next;
	struct usb_dynamic_desc	*prev;
};

/* USB dynamic string descriptor structure */
struct usb_dynamic_string {
	uint8_t		id;
	uint16_t	lang;
	char		*str;
	uint16_t	len;
	uint16_t	flags;
};

/* USB dynamic configuration structure */
struct usb_dynamic_config {
	uint8_t			config_value;
	uint16_t		descriptor_size;
	struct usb_config_descriptor config;
	struct usb_dynamic_desc	*interfaces;
	int			num_interfaces;
	uint16_t		flags;
};

/* USB dynamic interface structure */
struct usb_dynamic_interface {
	uint8_t			number;
	uint8_t			alt_setting;
	struct usb_interface_descriptor desc;
	struct usb_dynamic_desc	*endpoints;
	int			num_endpoints;
	uint16_t		flags;
};

/* USB dynamic endpoint structure */
struct usb_dynamic_endpoint {
	uint8_t			address;
	struct usb_endpoint_descriptor desc;
	struct usb_endpoint_ss_companion_descriptor ss_comp;
	uint16_t		flags;
	uint8_t			has_ss_comp;
};

/* USB dynamic device structure - minimal stub */
struct usb_dynamic_device {
	struct usb_device_descriptor	device_desc;
	struct usb_dynamic_config	*configs[USB_DYNAMIC_MAX_CONFIGS];
	struct usb_dynamic_string	strings[USB_DYNAMIC_MAX_STRINGS];
	int				num_configs;
	int				num_strings;
	uint16_t			flags;
	struct mtx			mtx;
};

/* USB dynamic pool structure */
struct usb_dynamic_pool {
	void		*base;
	size_t		size;
	size_t		used;
	struct mtx	mtx;
};

/* USB dynamic descriptor initialization - stub */
static __inline int
usb_dynamic_init(struct usb_dynamic_device *dyn, const struct usb_device_descriptor *desc)
{
	if (dyn == NULL)
		return USB_ERR_INVAL;
	memset(dyn, 0, sizeof(*dyn));
	if (desc)
		dyn->device_desc = *desc;
	mtx_init(&dyn->mtx, "usb_dyn", NULL, MTX_DEF);
	return 0;
}

/* USB dynamic descriptor cleanup - stub */
static __inline void
usb_dynamic_cleanup(struct usb_dynamic_device *dyn)
{
	if (dyn)
		mtx_destroy(&dyn->mtx);
}

/* USB dynamic add configuration - stub */
static __inline int
usb_dynamic_add_config(struct usb_dynamic_device *dyn,
    const struct usb_config_descriptor *config)
{
	int idx;
	if (dyn == NULL || config == NULL)
		return USB_ERR_INVAL;
	if (dyn->num_configs >= USB_DYNAMIC_MAX_CONFIGS)
		return USB_ERR_NO_RESOURCES;
	idx = dyn->num_configs++;
	dyn->configs[idx] = (struct usb_dynamic_config *)config;
	return 0;
}

/* USB dynamic remove configuration - stub */
static __inline int
usb_dynamic_remove_config(struct usb_dynamic_device *dyn, uint8_t config_value)
{
	(void)dyn;
	(void)config_value;
	return USB_ERR_NORMAL_COMPLETION;
}

/* USB dynamic get configuration - stub */
static __inline struct usb_dynamic_config *
usb_dynamic_get_config(struct usb_dynamic_device *dyn, uint8_t config_value)
{
	int i;
	if (dyn == NULL)
		return NULL;
	for (i = 0; i < dyn->num_configs; i++) {
		if (dyn->configs[i] &&
		    dyn->configs[i]->config_value == config_value)
			return dyn->configs[i];
	}
	return NULL;
}

/* USB dynamic add interface - stub */
static __inline int
usb_dynamic_add_interface(struct usb_dynamic_config *config,
    const struct usb_interface_descriptor *iface)
{
	(void)config;
	(void)iface;
	return USB_ERR_NORMAL_COMPLETION;
}

/* USB dynamic remove interface - stub */
static __inline int
usb_dynamic_remove_interface(struct usb_dynamic_config *config, uint8_t num)
{
	(void)config;
	(void)num;
	return USB_ERR_NORMAL_COMPLETION;
}

/* USB dynamic add endpoint - stub */
static __inline int
usb_dynamic_add_endpoint(struct usb_dynamic_interface *iface,
    const struct usb_endpoint_descriptor *ep)
{
	(void)iface;
	(void)ep;
	return USB_ERR_NORMAL_COMPLETION;
}

/* USB dynamic remove endpoint - stub */
static __inline int
usb_dynamic_remove_endpoint(struct usb_dynamic_interface *iface, uint8_t addr)
{
	(void)iface;
	(void)addr;
	return USB_ERR_NORMAL_COMPLETION;
}

/* USB dynamic add string - stub */
static __inline int
usb_dynamic_add_string(struct usb_dynamic_device *dyn, uint8_t id,
    uint16_t lang, const char *str)
{
	int idx;
	if (dyn == NULL || str == NULL)
		return USB_ERR_INVAL;
	if (dyn->num_strings >= USB_DYNAMIC_MAX_STRINGS)
		return USB_ERR_NO_RESOURCES;
	idx = dyn->num_strings++;
	dyn->strings[idx].id = id;
	dyn->strings[idx].lang = lang;
	dyn->strings[idx].str = (char *)str;  /* Assume managed elsewhere */
	dyn->strings[idx].len = strlen(str);
	return 0;
}

/* USB dynamic remove string - stub */
static __inline int
usb_dynamic_remove_string(struct usb_dynamic_device *dyn, uint8_t id, uint16_t lang)
{
	(void)dyn;
	(void)id;
	(void)lang;
	return USB_ERR_NORMAL_COMPLETION;
}

/* USB dynamic get string - stub */
static __inline const char *
usb_dynamic_get_string(struct usb_dynamic_device *dyn, uint8_t id, uint16_t lang)
{
	int i;
	if (dyn == NULL)
		return NULL;
	for (i = 0; i < dyn->num_strings; i++) {
		if (dyn->strings[i].id == id && dyn->strings[i].lang == lang)
			return dyn->strings[i].str;
	}
	return NULL;
}

/* USB dynamic build config descriptor - stub */
static __inline int
usb_dynamic_build_config_desc(struct usb_dynamic_config *config,
    void *buf, size_t buflen, uint8_t speed)
{
	(void)config;
	(void)buf;
	(void)buflen;
	(void)speed;
	return 0;
}

/* USB dynamic build full config - stub */
static __inline int
usb_dynamic_build_full_config(struct usb_dynamic_device *dyn,
    uint8_t config_value, void *buf, size_t buflen)
{
	(void)dyn;
	(void)config_value;
	(void)buf;
	(void)buflen;
	return 0;
}

/* USB dynamic get descriptor size - stub */
static __inline size_t
usb_dynamic_get_desc_size(struct usb_dynamic_config *config)
{
	if (config == NULL)
		return 0;
	return config->descriptor_size;
}

/* USB dynamic set device descriptor - stub */
static __inline int
usb_dynamic_set_device_desc(struct usb_dynamic_device *dyn,
    const struct usb_device_descriptor *desc)
{
	if (dyn == NULL || desc == NULL)
		return USB_ERR_INVAL;
	dyn->device_desc = *desc;
	return 0;
}

/* USB dynamic get device descriptor - stub */
static __inline const struct usb_device_descriptor *
usb_dynamic_get_device_desc(struct usb_dynamic_device *dyn)
{
	if (dyn == NULL)
		return NULL;
	return &dyn->device_desc;
}

/* USB dynamic set vendor/product - stub */
static __inline void
usb_dynamic_set_id(struct usb_dynamic_device *dyn, uint16_t vendor,
    uint16_t product, uint16_t bcd_device)
{
	if (dyn == NULL)
		return;
	dyn->device_desc.idVendor = vendor;
	dyn->device_desc.idProduct = product;
	dyn->device_desc.bcdDevice = bcd_device;
}

/* USB dynamic set class - stub */
static __inline void
usb_dynamic_set_class(struct usb_dynamic_device *dyn, uint8_t class,
    uint8_t subclass, uint8_t protocol)
{
	if (dyn == NULL)
		return;
	dyn->device_desc.bDeviceClass = class;
	dyn->device_desc.bDeviceSubClass = subclass;
	dyn->device_desc.bDeviceProtocol = protocol;
}

/* USB dynamic pool initialization - stub */
static __inline int
usb_dynamic_pool_init(struct usb_dynamic_pool *pool, size_t size)
{
	if (pool == NULL)
		return USB_ERR_INVAL;
	memset(pool, 0, sizeof(*pool));
	pool->size = size;
	mtx_init(&pool->mtx, "usb_pool", NULL, MTX_DEF);
	return 0;
}

/* USB dynamic pool cleanup - stub */
static __inline void
usb_dynamic_pool_cleanup(struct usb_dynamic_pool *pool)
{
	if (pool)
		mtx_destroy(&pool->mtx);
}

/* USB dynamic pool allocate - stub */
static __inline void *
usb_dynamic_pool_alloc(struct usb_dynamic_pool *pool, size_t size)
{
	(void)pool;
	(void)size;
	return NULL;
}

/* USB dynamic pool free - stub */
static __inline void
usb_dynamic_pool_free(struct usb_dynamic_pool *pool, void *ptr)
{
	(void)pool;
	(void)ptr;
}

/* USB dynamic lock/unlock - stubs */
static __inline void
usb_dynamic_lock(struct usb_dynamic_device *dyn)
{
	if (dyn)
		mtx_lock(&dyn->mtx);
}

static __inline void
usb_dynamic_unlock(struct usb_dynamic_device *dyn)
{
	if (dyn)
		mtx_unlock(&dyn->mtx);
}

/* USB dynamic descriptor allocation helpers - stubs */
static __inline struct usb_dynamic_desc *
usb_dynamic_alloc_desc(uint8_t type, uint16_t len)
{
	(void)type;
	(void)len;
	return NULL;
}

static __inline void
usb_dynamic_free_desc(struct usb_dynamic_desc *desc)
{
	(void)desc;
}

/* USB dynamic descriptor copy - stub */
static __inline int
usb_dynamic_copy_desc(struct usb_dynamic_desc *dst,
    const struct usb_dynamic_desc *src)
{
	if (dst == NULL || src == NULL)
		return USB_ERR_INVAL;
	*dst = *src;
	return 0;
}

/* USB dynamic find descriptor - stub */
static __inline struct usb_dynamic_desc *
usb_dynamic_find_desc(struct usb_dynamic_config *config, uint8_t type, uint8_t index)
{
	(void)config;
	(void)type;
	(void)index;
	return NULL;
}

/* USB dynamic count interfaces - stub */
static __inline int
usb_dynamic_count_interfaces(struct usb_dynamic_config *config)
{
	if (config == NULL)
		return 0;
	return config->num_interfaces;
}

/* USB dynamic count endpoints - stub */
static __inline int
usb_dynamic_count_endpoints(struct usb_dynamic_interface *iface)
{
	if (iface == NULL)
		return 0;
	return iface->num_endpoints;
}

#endif /* _USB_DYNAMIC_H_ */
