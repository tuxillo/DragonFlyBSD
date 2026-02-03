KMOD= wq_test40
SRCS= wq_test40.c \
 device_if.h \
 bus_if.h \
 pci_if.h \
 pcib_if.h

# Note: CFLAGS is reset by bsd.kmod.mk, so we must add AFTER it
.include <bsd.kmod.mk>

# Add LinuxKPI includes AFTER bsd.kmod.mk (CFLAGS is reset inside it)
CFLAGS+= -Idragonfly/compat/linuxkpi/common/include
CFLAGS+= -Idragonfly/compat/linuxkpi/dummy/include
CFLAGS+= -Idragonfly/usr.bin/dfregress
CFLAGS+= -include dragonfly/compat/linuxkpi/common/include/linux/kconfig.h

# LinuxKPI and DragonFly headers have conflicting macros
# Disable -Werror to allow compilation despite warnings
CWARNFLAGS= -Wall
WERROR=
