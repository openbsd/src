#	$Id: Makefile,v 1.3 1998/08/22 09:25:11 smurph Exp $

S=	${.CURDIR}/../../../..
INCL?= -I${.CURDIR} -I${.CURDIR}/../libsa -I${S}/lib/libsa -I${S}
COPTS?= ${DEFS} ${INCL}

.include "${S}/arch/mvme88k/stand/libsa/Makefile.inc"

SRCS=   sboot.c clock.c etherfun.c if_le.c

OBJS=	${SRCS:S/.c/.o/g}
CLEANFILES+=XBUG.o XSRT0.o oc_cksum.o sboot.tmp rboot.tmp srec sboot rboot
MDEC_DIR?=/usr/mdec

all:	sboot rboot

sboot.tmp:	XSRT0.o oc_cksum.o ${OBJS} ${LIBSA}
	ld -N -s -static -T 0x4000 XSRT0.o ${OBJS} oc_cksum.o -o sboot.tmp ${LIBSA}

rboot.tmp:	XBUG.o XSRT0.o oc_cksum.o ${OBJS} ${LIBSA}
	ld -N -s -static -Ttext 0xffa00000 -Tdata 0x4000 XBUG.o XSRT0.o \
	    ${OBJS} oc_cksum.o -o rboot.tmp ${LIBSA}

srec:	srec.c
	${CC} ${.CURDIR}/srec.c -o srec

sboot: sboot.tmp srec
	dd ibs=32 skip=1 if=sboot.tmp | ${.OBJDIR}/srec 4 0x4000 sboot > sboot

rboot: rboot.tmp srec
	dd ibs=32 skip=1 if=rboot.tmp | ${.OBJDIR}/srec 4 0x4000 rboot > rboot

install:
	${INSTALL} ${INSTALL_COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    sboot ${DESTDIR}${MDEC_DIR}/sboot
	${INSTALL} ${INSTALL_COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    rboot ${DESTDIR}${MDEC_DIR}/rboot

.include <bsd.prog.mk>
