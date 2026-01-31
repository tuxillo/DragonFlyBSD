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

#ifndef _USB_HUB_H_
#define _USB_HUB_H_

/* DragonFly USB hub definitions for LinuxKPI */

#include <sys/types.h>
#include <dev/usb/usb.h>

/* USB hub class constants */
#define USB_CLASS_HUB			0x09
#define USB_SUBCLASS_HUB		0x00
#define USB_PROTOCOL_HUB_FS		0x00
#define USB_PROTOCOL_HUB_HS_SINGLE_TT	0x01
#define USB_PROTOCOL_HUB_HS_MULTI_TT	0x02
#define USB_PROTOCOL_HUB_SS		0x03

/* Hub descriptor types */
#define USB_DESCRIPTOR_TYPE_HUB		0x29
#define USB_DESCRIPTOR_TYPE_SS_HUB	0x2A

/* Hub characteristics */
#define USB_HUB_CHARACTERISTIC_GANGED		0x00
#define USB_HUB_CHARACTERISTIC_INDIVIDUAL	0x01
#define USB_HUB_CHARACTERISTIC_NO_OC		0x00
#define USB_HUB_CHARACTERISTIC_GLOBAL_OC	0x01
#define USB_HUB_CHARACTERISTIC_INDIVIDUAL_OC	0x02

/* Hub feature selectors */
#define USB_HUB_FEATURE_C_HUB_LOCAL_POWER	0x00
#define USB_HUB_FEATURE_C_HUB_OVER_CURRENT	0x01

/* Port feature selectors */
#define USB_PORT_FEATURE_CONNECTION		0x00
#define USB_PORT_FEATURE_ENABLE			0x01
#define USB_PORT_FEATURE_SUSPEND		0x02
#define USB_PORT_FEATURE_OVER_CURRENT		0x03
#define USB_PORT_FEATURE_RESET			0x04
#define USB_PORT_FEATURE_POWER			0x08
#define USB_PORT_FEATURE_LOWSPEED		0x09
#define USB_PORT_FEATURE_C_CONNECTION		0x10
#define USB_PORT_FEATURE_C_ENABLE		0x11
#define USB_PORT_FEATURE_C_SUSPEND		0x12
#define USB_PORT_FEATURE_C_OVER_CURRENT		0x13
#define USB_PORT_FEATURE_C_RESET		0x14
#define USB_PORT_FEATURE_TEST			0x15
#define USB_PORT_FEATURE_INDICATOR		0x16
#define USB_PORT_FEATURE_U1_TIMEOUT		0x17
#define USB_PORT_FEATURE_U2_TIMEOUT		0x18
#define USB_PORT_FEATURE_C_PORT_LINK_STATE	0x19
#define USB_PORT_FEATURE_C_PORT_CONFIG_ERROR	0x1A
#define USB_PORT_FEATURE_C_BH_PORT_RESET	0x1B
#define USB_PORT_FEATURE_FORCE_LINKPM_ACCEPT	0x1C

/* Hub limits */
#define USB_HUB_MAX_PORTS			255
#define USB_HUB_MIN_PORTS			1
#define USB_HUB_MAX_DEPTH			5

/* Hub power */
#define USB_HUB_POWER_GOOD_TIME			100	/* ms */
#define USB_HUB_RESET_DELAY			10	/* ms */
#define USB_HUB_RESET_RECOVERY			10	/* ms */
#define USB_HUB_PORT_POWER_DELAY		100	/* ms */

/* USB hub descriptor structure */
struct usb_hub_descriptor {
	uint8_t	bDescLength;
	uint8_t	bDescriptorType;
	uint8_t	bNbrPorts;
	uint16_t	wHubCharacteristics;
	uint8_t	bPwrOn2PwrGood;
	uint8_t	bHubContrCurrent;
	uint8_t	DeviceRemovable[32];  /* Variable length */
};

/* USB SS hub descriptor structure */
struct usb_hub_ss_descriptor {
	uint8_t	bDescLength;
	uint8_t	bDescriptorType;
	uint8_t	bNbrPorts;
	uint16_t	wHubCharacteristics;
	uint8_t	bPwrOn2PwrGood;
	uint8_t	bHubContrCurrent;
	uint8_t	bHubHdrDecLat;
	uint16_t	wHubDelay;
	uint16_t	DeviceRemovable[16];  /* Variable length */
};

/* USB hub status structure */
struct usb_hub_status {
	uint16_t	wHubStatus;
	uint16_t	wHubChange;
};

/* Hub status bits */
#define USB_HUB_STATUS_LOCAL_POWER		0x0001
#define USB_HUB_STATUS_OVER_CURRENT		0x0002

/* Hub change bits */
#define USB_HUB_CHANGE_LOCAL_POWER		0x0001
#define USB_HUB_CHANGE_OVER_CURRENT		0x0002

/* USB port status structure */
struct usb_port_status {
	uint16_t	wPortStatus;
	uint16_t	wPortChange;
};

/* Port status bits */
#define USB_PORT_STATUS_CONNECTION		0x0001
#define USB_PORT_STATUS_ENABLE			0x0002
#define USB_PORT_STATUS_SUSPEND			0x0004
#define USB_PORT_STATUS_OVER_CURRENT		0x0008
#define USB_PORT_STATUS_RESET			0x0010
#define USB_PORT_STATUS_POWER			0x0100
#define USB_PORT_STATUS_LOW_SPEED		0x0200
#define USB_PORT_STATUS_HIGH_SPEED		0x0400
#define USB_PORT_STATUS_TEST			0x0800
#define USB_PORT_STATUS_INDICATOR		0x1000
#define USB_PORT_STATUS_LINK_STATE		0x1E00
#define USB_PORT_STATUS_U1_ENABLE		0x2000
#define USB_PORT_STATUS_U2_ENABLE		0x4000
#define USB_PORT_STATUS_BC_ENABLE		0x8000

/* Port change bits */
#define USB_PORT_CHANGE_CONNECTION		0x0001
#define USB_PORT_CHANGE_ENABLE			0x0002
#define USB_PORT_CHANGE_SUSPEND			0x0004
#define USB_PORT_CHANGE_OVER_CURRENT		0x0008
#define USB_PORT_CHANGE_RESET			0x0010
#define USB_PORT_CHANGE_LINK_STATE		0x0020
#define USB_PORT_CHANGE_CONFIG_ERROR		0x0040
#define USB_PORT_CHANGE_BH_RESET		0x0080

/* USB hub structure - minimal stub */
struct usb_hub {
	struct usb_device	*hub_udev;
	struct usb_device	**hub_ports;
	struct usb_xfer		*hub_xfer;
	struct usb_hub_descriptor	hub_desc;
	uint16_t		hub_portcount;
	uint16_t		hub_features;
	uint8_t			hub_tt;
	uint8_t			hub_tt_port;
	uint8_t			hub_is_multi_tt;
	struct mtx		hub_mtx;
};

/* USB hub softc structure */
struct usb_hub_softc {
	struct device		sc_dev;
	struct usb_hub		*sc_hub;
	int			sc_port;
	int			sc_enabled;
};

/* USB hub operations - stubs */
static __inline int
usb_hub_probe(device_t dev)
{
	(void)dev;
	return 0;
}

static __inline int
usb_hub_attach(device_t dev)
{
	(void)dev;
	return 0;
}

static __inline int
usb_hub_detach(device_t dev)
{
	(void)dev;
	return 0;
}

/* USB hub initialization - stub */
static __inline int
usb_hub_init(struct usb_hub *hub, struct usb_device *udev)
{
	if (hub == NULL || udev == NULL)
		return USB_ERR_INVAL;
	memset(hub, 0, sizeof(*hub));
	hub->hub_udev = udev;
	hub->hub_portcount = 1;  /* Minimum */
	mtx_init(&hub->hub_mtx, "usb_hub", NULL, MTX_DEF);
	return 0;
}

/* USB hub cleanup - stub */
static __inline void
usb_hub_uninit(struct usb_hub *hub)
{
	if (hub)
		mtx_destroy(&hub->hub_mtx);
}

/* USB hub get descriptor - stub */
static __inline int
usb_hub_get_descriptor(struct usb_device *udev, struct usb_hub_descriptor *desc)
{
	if (udev == NULL || desc == NULL)
		return USB_ERR_INVAL;
	memset(desc, 0, sizeof(*desc));
	desc->bDescLength = sizeof(struct usb_hub_descriptor);
	desc->bDescriptorType = USB_DESCRIPTOR_TYPE_HUB;
	desc->bNbrPorts = 1;
	return 0;
}

/* USB hub get status - stub */
static __inline int
usb_hub_get_status(struct usb_device *udev, struct usb_hub_status *status)
{
	if (udev == NULL || status == NULL)
		return USB_ERR_INVAL;
	memset(status, 0, sizeof(*status));
	return 0;
}

/* USB hub port status - stub */
static __inline int
usb_hub_get_port_status(struct usb_device *udev, uint8_t port,
    struct usb_port_status *status)
{
	(void)port;
	if (udev == NULL || status == NULL)
		return USB_ERR_INVAL;
	memset(status, 0, sizeof(*status));
	return 0;
}

/* USB hub set port feature - stub */
static __inline int
usb_hub_set_port_feature(struct usb_device *udev, uint8_t port,
    uint8_t feature)
{
	(void)udev;
	(void)port;
	(void)feature;
	return USB_ERR_NORMAL_COMPLETION;
}

/* USB hub clear port feature - stub */
static __inline int
usb_hub_clear_port_feature(struct usb_device *udev, uint8_t port,
    uint8_t feature)
{
	(void)udev;
	(void)port;
	(void)feature;
	return USB_ERR_NORMAL_COMPLETION;
}

/* USB hub reset port - stub */
static __inline int
usb_hub_reset_port(struct usb_device *udev, uint8_t port)
{
	(void)udev;
	(void)port;
	return USB_ERR_NORMAL_COMPLETION;
}

/* USB hub enable port - stub */
static __inline int
usb_hub_enable_port(struct usb_device *udev, uint8_t port)
{
	(void)udev;
	(void)port;
	return USB_ERR_NORMAL_COMPLETION;
}

/* USB hub disable port - stub */
static __inline int
usb_hub_disable_port(struct usb_device *udev, uint8_t port)
{
	(void)udev;
	(void)port;
	return USB_ERR_NORMAL_COMPLETION;
}

/* USB hub suspend port - stub */
static __inline int
usb_hub_suspend_port(struct usb_device *udev, uint8_t port)
{
	(void)udev;
	(void)port;
	return USB_ERR_NORMAL_COMPLETION;
}

/* USB hub resume port - stub */
static __inline int
usb_hub_resume_port(struct usb_device *udev, uint8_t port)
{
	(void)udev;
	(void)port;
	return USB_ERR_NORMAL_COMPLETION;
}

/* USB hub power on port - stub */
static __inline int
usb_hub_power_on_port(struct usb_device *udev, uint8_t port)
{
	(void)udev;
	(void)port;
	return USB_ERR_NORMAL_COMPLETION;
}

/* USB hub power off port - stub */
static __inline int
usb_hub_power_off_port(struct usb_device *udev, uint8_t port)
{
	(void)udev;
	(void)port;
	return USB_ERR_NORMAL_COMPLETION;
}

/* USB hub port connected - stub */
static __inline int
usb_hub_port_is_connected(struct usb_port_status *status)
{
	if (status == NULL)
		return 0;
	return status->wPortStatus & USB_PORT_STATUS_CONNECTION;
}

/* USB hub port enabled - stub */
static __inline int
usb_hub_port_is_enabled(struct usb_port_status *status)
{
	if (status == NULL)
		return 0;
	return status->wPortStatus & USB_PORT_STATUS_ENABLE;
}

/* USB hub port suspended - stub */
static __inline int
usb_hub_port_is_suspended(struct usb_port_status *status)
{
	if (status == NULL)
		return 0;
	return status->wPortStatus & USB_PORT_STATUS_SUSPEND;
}

/* USB hub port over current - stub */
static __inline int
usb_hub_port_is_overcurrent(struct usb_port_status *status)
{
	if (status == NULL)
		return 0;
	return status->wPortStatus & USB_PORT_STATUS_OVER_CURRENT;
}

/* USB hub port reset - stub */
static __inline int
usb_hub_port_is_reset(struct usb_port_status *status)
{
	if (status == NULL)
		return 0;
	return status->wPortStatus & USB_PORT_STATUS_RESET;
}

/* USB hub port powered - stub */
static __inline int
usb_hub_port_is_powered(struct usb_port_status *status)
{
	if (status == NULL)
		return 0;
	return status->wPortStatus & USB_PORT_STATUS_POWER;
}

/* USB hub port low speed - stub */
static __inline int
usb_hub_port_is_lowspeed(struct usb_port_status *status)
{
	if (status == NULL)
		return 0;
	return status->wPortStatus & USB_PORT_STATUS_LOW_SPEED;
}

/* USB hub port high speed - stub */
static __inline int
usb_hub_port_is_highspeed(struct usb_port_status *status)
{
	if (status == NULL)
		return 0;
	return status->wPortStatus & USB_PORT_STATUS_HIGH_SPEED;
}

/* USB hub port change connection - stub */
static __inline int
usb_hub_port_changed_connection(struct usb_port_status *status)
{
	if (status == NULL)
		return 0;
	return status->wPortChange & USB_PORT_CHANGE_CONNECTION;
}

/* USB hub port change enable - stub */
static __inline int
usb_hub_port_changed_enable(struct usb_port_status *status)
{
	if (status == NULL)
		return 0;
	return status->wPortChange & USB_PORT_CHANGE_ENABLE;
}

/* USB hub port change reset - stub */
static __inline int
usb_hub_port_changed_reset(struct usb_port_status *status)
{
	if (status == NULL)
		return 0;
	return status->wPortChange & USB_PORT_CHANGE_RESET;
}

/* USB hub explore - stub */
static __inline void
usb_hub_explore(struct usb_hub *hub)
{
	(void)hub;
}

/* USB hub interrupt callback - stub */
static __inline void
usb_hub_interrupt_callback(struct usb_xfer *xfer)
{
	(void)xfer;
}

#endif /* _USB_HUB_H_ */
