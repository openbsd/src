#	$OpenBSD: Makefile,v 1.5 1996/05/16 02:55:35 chuck Exp $

RELOC=0x3F0000

S=	${.CURDIR}/../../../..
DEFS= -DSUN_BOOTPARAMS
INCPATH=-I${.CURDIR} -I${.CURDIR}/../libsa -I${.CURDIR}/../libbug \
	-I${S} -I${S}/lib/libsa
CFLAGS=	-O2 ${INCPATH} ${DEFS} ${COPTS}
CLEANFILES+=netboot netboot.bin

.include "${S}/arch/${MACHINE}/stand/bugcrt/Makefile.inc"
.include "${S}/arch/${MACHINE}/stand/libbug/Makefile.inc"
.include "${S}/arch/${MACHINE}/stand/libsa/Makefile.inc"

SRCS=   boot.c conf.c version.c devopen.c dev_net.c
SRCS+=	if_ie.c if_le.c
OBJS=  ${SRCS:S/.c/.o/g}

all:	netboot.bin

netboot: ${OBJS} ${LIBSA} ${BUGCRT} ${LIBBUG}
	${LD} -s -N -T ${RELOC} -o $@ \
		${SRTOBJ} ${BUGCRT} ${OBJS} ${LIBSA} ${LIBBUG}
	@size $@

netboot.bin: netboot
	dd ibs=32 skip=1 if=netboot of=$@

install:
	install ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	  netboot.bin ${DESTDIR}${MDEC_DIR}/netboot

.include <bsd.prog.mk>
