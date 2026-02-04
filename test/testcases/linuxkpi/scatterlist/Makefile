PROG=	linuxkpi_scatterlist
SRCS=	linuxkpi_scatterlist.c
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
