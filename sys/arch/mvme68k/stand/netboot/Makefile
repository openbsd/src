#	$OpenBSD: Makefile,v 1.10 2000/01/24 05:20:56 smurph Exp $

RELOC=0x3F0000
SIZE?=	size

S=	${.CURDIR}/../../../..
DEFS= -DSUN_BOOTPARAMS -DRELOC=${RELOC}
INCPATH=-I${.CURDIR} -I${.CURDIR}/../libsa -I${.CURDIR}/../libbug \
	-I${S} -I${S}/lib/libsa
CFLAGS=	-O2 ${DEFS} ${INCPATH} ${COPTS} -m68060
CLEANFILES+=netboot netboot.bin

.include "${S}/arch/mvme68k/stand/bugcrt/Makefile.inc"
.include "${S}/arch/mvme68k/stand/libbug/Makefile.inc"
.include "${S}/arch/mvme68k/stand/libsa/Makefile.inc"
.include "${S}/arch/mvme68k/stand/libz/Makefile.inc"

SRCS=   boot.c conf.c version.c devopen.c dev_net.c
SRCS+=	if_ie.c if_le.c
OBJS=  ${SRCS:S/.c/.o/g}
LIBS= ${LIBSA} ${LIBBUG} ${LIBZ}
LDFLAGS+= -nostdlib -s -N -T ${RELOC}

all:	netboot.bin

netboot: ${OBJS} ${BUGCRT} ${LIBS}
	${LD} ${LDFLAGS} -o $@ \
		${SRTOBJ} ${BUGCRT} ${OBJS} ${LIBS}
	@${SIZE} $@

netboot.bin: netboot
	dd ibs=32 skip=1 if=netboot of=$@

install:
	${INSTALL} ${INSTALL_COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	  netboot.bin ${DESTDIR}${MDEC_DIR}/netboot

.include <bsd.prog.mk>
