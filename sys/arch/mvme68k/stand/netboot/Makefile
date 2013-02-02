#	$OpenBSD: Makefile,v 1.18 2013/02/02 13:36:06 miod Exp $

SIZE?=	size
STRIP?=	strip

S=	${.CURDIR}/../../../..
DEFS= -DSUN_BOOTPARAMS
#-DNETIF_DEBUG
INCPATH=-I${.CURDIR} -I${.CURDIR}/../libsa -I${.CURDIR}/../libbug \
	-I${S} -I${S}/lib/libsa
CFLAGS+=-O2 ${DEFS} ${INCPATH} ${COPTS}
CLEANFILES+=netboot netboot.bin netboot.strip

.include "${S}/arch/mvme68k/stand/bugcrt/Makefile.inc"
.include "${S}/arch/mvme68k/stand/libbug/Makefile.inc"
.include "${S}/arch/mvme68k/stand/libsa/Makefile.inc"
.include "${S}/arch/mvme68k/stand/libz/Makefile.inc"

SRCS=   boot.c conf.c version.c devopen.c dev_net.c
SRCS+=	if_ie.c if_le.c
OBJS=  ${SRCS:S/.c/.o/g}
LIBS= ${LIBSA} ${LIBBUG} ${LIBZ}
LDFLAGS+=-N -T ${S}/arch/mvme68k/stand/ld.script -Ttext ${STAGE2_RELOC}
NOPIE=

all:	netboot.bin

netboot: ${SRTOBJ} ${BUGCRT} ${OBJS} ${LIBS}
	${LD} ${LDFLAGS} -o $@ \
	    ${SRTOBJ} ${BUGCRT} ${OBJS} ${LIBS} `${CC} -print-libgcc-file-name`

netboot.bin: netboot
	${STRIP} -I a.out-m68k-netbsd -o netboot.strip netboot
	dd ibs=32 skip=1 if=netboot.strip of=$@

install:
	${INSTALL} ${INSTALL_COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	  netboot.bin ${DESTDIR}${MDEC_DIR}/netboot

.include <bsd.prog.mk>
