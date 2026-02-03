KMOD=\twq_test44
SRCS=\twq_test44.c \
\tdevice_if.h \
\tbus_if.h \
\tpci_if.h \
\tpcib_if.h

# Note: CFLAGS is reset by bsd.kmod.mk, so we must add AFTER it
.include <bsd.kmod.mk>

# Add LinuxKPI includes AFTER bsd.kmod.mk (CFLAGS is reset inside it)
CFLAGS+=\t-Idragonfly/compat/linuxkpi/common/include
CFLAGS+=\t-Idragonfly/compat/linuxkpi/dummy/include
CFLAGS+=\t-Idragonfly/usr.bin/dfregress
CFLAGS+=\t-include dragonfly/compat/linuxkpi/common/include/linux/kconfig.h

# LinuxKPI and DragonFly headers have conflicting macros
# Disable -Werror to allow compilation despite warnings
CWARNFLAGS=\t-Wall
WERROR=\t	
