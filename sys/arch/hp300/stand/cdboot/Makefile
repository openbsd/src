#	$OpenBSD: Makefile,v 1.8 2013/02/02 13:34:29 miod Exp $

PROG=	cdboot
OBJCOPY?=objcopy
SIZE?=	size

NOMAN=
NOPIE=

CPPFLAGS+=	${SA_CPPFLAGS}

.PATH:	${.CURDIR}/../uboot

CFLAGS+=-DOSREV=\"${OSREV}\" -I${.CURDIR}/../include \
	-I${.CURDIR}/../libsa -I${.CURDIR}/../.. -I${.CURDIR}/../../.. \
	-I${.CURDIR}/../../../..

AFLAGS=	-x assembler-with-cpp -traditional-cpp -I${.CURDIR}/../include \
	-I${.CURDIR}/../libsa -I${.CURDIR}/../.. -I${.CURDIR}/../../.. \
	-I${.CURDIR}/../../../..

SRCS=	srt0.S cdboot.c clock.c conf.c cons.c devopen.c

LIBS=	${LIBCOMMON} ${LIBSA} ${LIBZ}

CLEANFILES+=	${PROG}.bin ${PROG}.lif

cdboot.lif:	${PROG}
	${OBJCOPY} -S -O binary ${PROG} ${PROG}.bin
	${MKBOOT_PROG} -l 0x${RELOC} ${PROG}.bin $@

cdboot:	${OBJS} ${LIBS}
	${LD} -N -Ttext ${RELOC} -e begin --warn-common ${OBJS} ${LIBS} -o $@
	@${SIZE} $@
	@echo $@ total size may not exceed 30000 bytes

install:	cdboot.lif ${MKBOOT_PROG}
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 cdboot.lif \
	    ${DESTDIR}/usr/mdec

cdboot.o:	Makefile

.include <bsd.prog.mk>
