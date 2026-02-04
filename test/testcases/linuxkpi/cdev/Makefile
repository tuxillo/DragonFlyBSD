PROG= 	linuxkpi_cdev
SRCS= 	linuxkpi_cdev.c
NOMAN=

.MAIN: all

.include <bsd.prog.mk>

all: kmod

kmod:
	${MAKE} -C kmod all

clean: kmod-clean

kmod-clean:
	${MAKE} -C kmod clean
