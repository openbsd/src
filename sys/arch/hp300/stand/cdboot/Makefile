#	$OpenBSD: Makefile,v 1.6 2012/08/21 14:46:18 pascal Exp $

PROG=	cdboot

NOMAN=
NOPIE=

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${.CURDIR}/../uboot

CFLAGS+=-DOSREV=\"${OSREV}\" -I${.CURDIR}/../include \
	-I${.CURDIR}/../libsa -I${.CURDIR}/../.. -I${.CURDIR}/../../.. \
	-I${.CURDIR}/../../../..

AFLAGS=	-x assembler-with-cpp -traditional-cpp -I${.CURDIR}/../include \
	-I${.CURDIR}/../libsa -I${.CURDIR}/../.. -I${.CURDIR}/../../.. \
	-I${.CURDIR}/../../../..

SRCS=	srt0.S cdboot.c clock.c conf.c cons.c devopen.c

LIBS=	${LIBCOMMON} ${LIBSA} ${LIBZ}

CLEANFILES+=	cdboot.lif

cdboot.lif:	cdboot
	${MKBOOT_PROG} cdboot $@

cdboot:	${OBJS} ${LIBS}
	${LD} -N -T ${RELOC} -e begin ${OBJS} ${LIBS} -o $@
	@size $@
	@echo $@ total size may not exceed 30000 bytes

install:	cdboot.lif ${MKBOOT_PROG}
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 cdboot.lif \
	    ${DESTDIR}/usr/mdec

cdboot.o:	Makefile

.include <bsd.prog.mk>
