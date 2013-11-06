#	$OpenBSD: Makefile,v 1.9 2013/11/06 19:37:06 miod Exp $

STRIP?= strip

S=	${.CURDIR}/../../../..
DEFS=
#DEFS+=-DNETIF_DEBUG
INCPATH=-I${.CURDIR} -I${.CURDIR}/../libsa -I${.CURDIR}/../libbug \
	-I${S} -I${S}/lib/libsa
CFLAGS+=${DEFS} ${INCPATH} ${COPTS}
CLEANFILES+=tftpboot tftpboot.bin

.include "${S}/arch/mvme88k/stand/bugcrt/Makefile.inc"
.include "${S}/arch/mvme88k/stand/libbug/Makefile.inc"
.include "${S}/arch/mvme88k/stand/libsa/Makefile.inc"
.include "${S}/arch/mvme88k/stand/libz/Makefile.inc"

SRCS=   boot.c conf.c version.c tftpfs.c netdev.c
OBJS=  ${SRCS:S/.c/.o/g}
LIBS= ${LIBSA} ${LIBBUG} ${LIBZ}
LDFLAGS=-N -T ${S}/arch/mvme88k/stand/ld.script -Ttext ${STAGE2_RELOC}
LDFLAGS+= -warn-common
NOPIE=

all:	tftpboot.bin

tftpboot: ${NETBOOT_CRT} ${OBJS} ${LIBS}
	${LD} ${LDFLAGS} -o $@ \
	     ${NETBOOT_CRT} ${OBJS} ${LIBS} `${CC} -print-libgcc-file-name`

tftpboot.bin: tftpboot
	${STRIP} tftpboot
	dd ibs=32 skip=1 if=tftpboot of=$@

install:
	${INSTALL} ${INSTALL_COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	  tftpboot.bin ${DESTDIR}${MDEC_DIR}/tftpboot

.include <bsd.prog.mk>
