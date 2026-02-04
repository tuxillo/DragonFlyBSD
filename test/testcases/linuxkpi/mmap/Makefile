PROG= 	linuxkpi_mmap
SRCS= 	linuxkpi_mmap.c
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
