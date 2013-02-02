#	$OpenBSD: Makefile,v 1.13 2013/02/02 13:36:06 miod Exp $

S=	${.CURDIR}/../../../..
INCL?= -I${.CURDIR} -I${.CURDIR}/../libsa -I${S}/lib/libsa -I${S}
HOSTCFLAGS?= ${CFLAGS}
CFLAGS+=-D_STANDALONE
CFLAGS+=-DSTAGE2_RELOC=${STAGE2_RELOC}
COPTS?= ${DEFS} ${INCL}

.include "${S}/arch/mvme68k/stand/libbug/Makefile.inc"
.include "${S}/arch/mvme68k/stand/libsa/Makefile.inc"
.include "${S}/arch/mvme68k/stand/libz/Makefile.inc"

SRCS=   sboot.c clock.c etherfun.c if_le.c

OBJS=	${SRCS:S/.c/.o/g}
LIBS=	${LIBSA} ${LIBBUG} ${LIBZ}
LDFLAGS= -static -s -N -T ${S}/arch/mvme68k/stand/ld.script
CLEANFILES+=XBUG.o XSRT0.o oc_cksum.o sboot.tmp rboot.tmp srec sboot rboot
MDEC_DIR?=/usr/mdec
NOPIE=

all:	sboot rboot

sboot.tmp:	XSRT0.o oc_cksum.o ${OBJS} ${LIBS}
	${LD} ${LDFLAGS} -Ttext ${STAGE1_RELOC} XSRT0.o ${OBJS} oc_cksum.o -o $@ \
	    ${LIBS} `${CC} -print-libgcc-file-name`

rboot.tmp:	XBUG.o XSRT0.o oc_cksum.o ${OBJS} ${LIBS}
	${LD} ${LDFLAGS} -Ttext 0xffa00000 -Tdata 0x4000 XBUG.o XSRT0.o \
	    ${OBJS} oc_cksum.o -o $@ ${LIBS} `${CC} -print-libgcc-file-name`

srec:	srec.c
	${HOSTCC} ${HOSTCFLAGS} -o $@ ${.CURDIR}/srec.c 

sboot: sboot.tmp srec
	dd ibs=32 skip=1 if=sboot.tmp | ${.OBJDIR}/srec 4 ${STAGE1_RELOC} sboot > sboot

rboot: rboot.tmp srec
	dd ibs=32 skip=1 if=rboot.tmp | ${.OBJDIR}/srec 4 0x4000 rboot > rboot

install:
	${INSTALL} ${INSTALL_COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    sboot ${DESTDIR}${MDEC_DIR}/sboot
	${INSTALL} ${INSTALL_COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    rboot ${DESTDIR}${MDEC_DIR}/rboot

.include <bsd.prog.mk>
