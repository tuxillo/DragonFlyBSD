# $FreeBSD: src/sys/modules/sound/driver/emu10k1/Makefile,v 1.4 2004/01/11 10:30:56 obrien Exp $

.PATH: ${.CURDIR}/../../pci \
       ${.CURDIR}/../../../../gnu/dev/sound/pci

KMOD=	snd_emu10k1
SRCS=	device_if.h bus_if.h pci_if.h emu10k1-alsa%diked.h
SRCS+=	emu10k1.c

CLEANFILES+= emu10k1-alsa%diked.h

emu10k1-alsa%diked.h: emu10k1-alsa.h
	grep -v '#include' ${.OODATE} | $(CC) -E -D__KERNEL__ -dM - \
	    | awk -F"[ 	(]" '/define/ \
	    { print "#ifndef " $$2 ; print ; print "#endif" }' \
	    >${.TARGET}

.include <bsd.kmod.mk>
