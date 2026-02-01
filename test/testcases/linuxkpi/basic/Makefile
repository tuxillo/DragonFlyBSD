KMOD=	linuxkpi_basic
SRCS=	linuxkpi_basic.c \
	device_if.h \
	bus_if.h \
	pci_if.h \
	pcib_if.h

# Note: CFLAGS is reset by bsd.kmod.mk, so we must add AFTER it
.include <bsd.kmod.mk>

# Add LinuxKPI includes AFTER bsd.kmod.mk (CFLAGS is reset inside it)
CFLAGS+=	-Idragonfly/compat/linuxkpi/common/include
CFLAGS+=	-Idragonfly/compat/linuxkpi/dummy/include
CFLAGS+=	-include dragonfly/compat/linuxkpi/common/include/linux/kconfig.h

# Suppress macro conflicts between LinuxKPI and DragonFly headers
CFLAGS+=	-Wno-error=macro-redefined
CFLAGS+=	-Wno-error=discarded-qualifiers
