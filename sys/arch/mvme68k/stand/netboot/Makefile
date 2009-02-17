#	$OpenBSD: Makefile,v 1.15 2009/02/17 18:42:06 miod Exp $

SIZE?=	size

S=	${.CURDIR}/../../../..
DEFS= -DSUN_BOOTPARAMS -DRELOC=${STAGE2_RELOC}
INCPATH=-I${.CURDIR} -I${.CURDIR}/../libsa -I${.CURDIR}/../libbug \
	-I${S} -I${S}/lib/libsa
CFLAGS+=	-O2 ${DEFS} ${INCPATH} ${COPTS}
CFLAGS+=-DSTAGE2_RELOC=${STAGE2_RELOC}
CLEANFILES+=netboot netboot.bin

.include "${S}/arch/mvme68k/stand/bugcrt/Makefile.inc"
.include "${S}/arch/mvme68k/stand/libbug/Makefile.inc"
.include "${S}/arch/mvme68k/stand/libsa/Makefile.inc"
.include "${S}/arch/mvme68k/stand/libz/Makefile.inc"

SRCS=   boot.c conf.c version.c devopen.c dev_net.c
SRCS+=	if_ie.c if_le.c
OBJS=  ${SRCS:S/.c/.o/g}
LIBS= ${LIBSA} ${LIBBUG} ${LIBZ}
LDFLAGS+= -nostdlib -s -N -T ${STAGE2_RELOC}

all:	netboot.bin

netboot: ${OBJS} ${BUGCRT} ${LIBS}
	${LD} ${LDFLAGS} -o $@ \
		${SRTOBJ} ${BUGCRT} ${OBJS} ${LIBS} `cc -print-libgcc-file-name`
	@${SIZE} $@

netboot.bin: netboot
	dd ibs=32 skip=1 if=netboot of=$@

install:
	${INSTALL} ${INSTALL_COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	  netboot.bin ${DESTDIR}${MDEC_DIR}/netboot

.include <bsd.prog.mk>
