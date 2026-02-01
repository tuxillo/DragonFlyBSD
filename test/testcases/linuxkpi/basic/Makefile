KMOD=	linuxkpi_basic
SRCS=	linuxkpi_basic.c \
	device_if.h \
	bus_if.h \
	pci_if.h \
	pcib_if.h

.include <bsd.kmod.mk>

# Add LinuxKPI include paths (must be after bsd.kmod.mk)
CFLAGS+=	-Idragonfly/compat/linuxkpi/common/include
CFLAGS+=	-Idragonfly/compat/linuxkpi/dummy/include
# Note: kconfig.h is included FIRST in the .c file, not here
