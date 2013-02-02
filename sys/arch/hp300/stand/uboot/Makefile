#	$OpenBSD: Makefile,v 1.9 2013/02/02 13:34:29 miod Exp $

PROG=	uboot
OBJCOPY?=	objcopy
SIZE?=	size

NOMAN=
NOPIE=

CPPFLAGS+=	${SA_CPPFLAGS}

CFLAGS+=-I${.CURDIR}/../include \
	-I${.CURDIR}/../libsa -I${.CURDIR}/../.. -I${.CURDIR}/../../.. \
	-I${.CURDIR}/../../../..

AFLAGS=	-x assembler-with-cpp -traditional-cpp -I${.CURDIR}/../include \
	-I${.CURDIR}/../libsa -I${.CURDIR}/../.. -I${.CURDIR}/../../.. \
	-I${.CURDIR}/../../../..

SRCS=	srt0.S uboot.c tgets.c netio.c clock.c conf.c prf.c

LIBS=	${LIBCOMMON} ${LIBSAZ} ${LIBZ}

CLEANFILES+=	${PROG}.bin ${PROG}.lif

uboot.lif:	${PROG}
	${OBJCOPY} -S -O binary ${PROG} ${PROG}.bin
	${MKBOOT_PROG} -l 0x${RELOC} ${PROG}.bin $@

uboot:	${OBJS} ${LIBS}
	${LD} -N -Ttext ${RELOC} -e begin --warn-common ${OBJS} ${LIBS} \
	    `${CC} -print-libgcc-file-name` -o $@
	@${SIZE} $@
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
