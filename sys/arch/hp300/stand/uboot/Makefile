#	$OpenBSD: Makefile,v 1.4 2005/02/20 15:33:30 miod Exp $

PROG=	uboot

NOMAN=

CFLAGS= -O2 -msoft-float -I${.CURDIR}/../include \
	-I${.CURDIR}/../libsa -I${.CURDIR}/../.. -I${.CURDIR}/../../.. \
	-I${.CURDIR}/../../../..

AFLAGS=	-x assembler-with-cpp -traditional-cpp -I${.CURDIR}/../include \
	-I${.CURDIR}/../libsa -I${.CURDIR}/../.. -I${.CURDIR}/../../.. \
	-I${.CURDIR}/../../../..

SRCS=	srt0.S uboot.c tgets.c netio.c clock.c conf.c

LIBS=	${LIBCOMMON} ${LIBSAZ} ${LIBZ} ${LIBKERN}

${OBJS}:	${.CURDIR}/../Makefile.inc

CLEANFILES+=	uboot.lif

uboot.lif:	uboot
	${MKBOOT_PROG} uboot $@

uboot:	${OBJS} ${LIBS}
	${LD} -N -T ${RELOC} -e begin ${OBJS} ${LIBS} \
	    `${CC} -print-libgcc-file-name` -o $@
	@size $@
	@echo $@ total size should not exceed 1044480 bytes

install:	uboot.lif ${MKBOOT_PROG}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	    ${DESTDIR}/usr/mdec/rbootd
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 uboot.lif \
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
