#	$OpenBSD: Makefile,v 1.2 1997/09/14 12:54:29 downsj Exp $

PROG=	uboot

NOMAN=

CFLAGS= -O3 -msoft-float -I${.CURDIR}/../include \
	-I${.CURDIR}/../libsa -I${.CURDIR}/../.. -I${.CURDIR}/../../.. \
	-I${.CURDIR}/../../../..

AFLAGS=	-x assembler-with-cpp -traditional-cpp -I${.CURDIR}/../include \
	-I${.CURDIR}/../libsa -I${.CURDIR}/../.. -I${.CURDIR}/../../.. \
	-I${.CURDIR}/../../../..

SRCS=	srt0.S uboot.c tgets.c netio.c clock.c conf.c

LIBS=	${LIBCOMMON} ${LIBSA} ${LIBZ} ${LIBKERN}

${OBJS}:	${.CURDIR}/../Makefile.inc

CLEANFILES+=	uboot.lif uboot.raw

uboot.lif:	uboot
	${MKBOOT_PROG} uboot $@

uboot.raw:	uboot
	${STRIPBOOT_PROG} uboot $@

uboot:	${OBJS} ${LIBS}
	${LD} -N -T ${RELOC} -e begin ${OBJS} ${LIBS} -o $@
	@size $@
	@echo $@ total size should not exceed 1044480 bytes

install:	uboot.lif uboot.raw ${MKBOOT_PROG}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	    ${DESTDIR}/usr/mdec/rbootd
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 uboot.lif \
	    ${DESTDIR}/usr/mdec
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 uboot.raw \
	    ${DESTDIR}/usr/mdec
	rm -f ${DESTDIR}/usr/mdec/hdboot
	ln ${DESTDIR}/usr/mdec/uboot.lif ${DESTDIR}/usr/mdec/hdboot
	rm -f ${DESTDIR}/usr/mdec/boothd
	ln ${DESTDIR}/usr/mdec/uboot.lif ${DESTDIR}/usr/mdec/boothd
	rm -f ${DESTDIR}/usr/mdec/sdboot
	ln ${DESTDIR}/usr/mdec/uboot.lif ${DESTDIR}/usr/mdec/sdboot
	rm -f ${DESTDIR}/usr/mdec/bootsd
	ln ${DESTDIR}/usr/mdec/uboot.lif ${DESTDIR}/usr/mdec/bootsd
	rm -f ${DESTDIR}/usr/mdec/ctboot
	ln ${DESTDIR}/usr/mdec/uboot.lif ${DESTDIR}/usr/mdec/ctboot
	rm -f ${DESTDIR}/usr/mdec/bootct
	ln ${DESTDIR}/usr/mdec/uboot.lif ${DESTDIR}/usr/mdec/bootct
	rm -f ${DESTDIR}/usr/mdec/rbootd/SYS_UBOOT
	ln ${DESTDIR}/usr/mdec/uboot.lif ${DESTDIR}/usr/mdec/rbootd/SYS_UBOOT

.include <bsd.prog.mk>
