PROG=	linuxkpi_vm_radix
SRCS=	linuxkpi_vm_radix.c
NOMAN=

.MAIN: all
.PHONY: kmod kmod-clean

.include <bsd.prog.mk>

all: kmod

kmod:
	${MAKE} -C kmod all

clean: kmod-clean

kmod-clean:
	${MAKE} -C kmod clean
