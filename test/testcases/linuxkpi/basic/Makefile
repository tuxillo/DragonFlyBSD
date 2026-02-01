KMOD=	linuxkpi_basic
SRCS=	linuxkpi_basic.c

.include <bsd.kmod.mk>

# Add LinuxKPI include paths (must be after bsd.kmod.mk)
CFLAGS+=	-Idragonfly/compat/linuxkpi/common/include
CFLAGS+=	-Idragonfly/compat/linuxkpi/dummy/include
