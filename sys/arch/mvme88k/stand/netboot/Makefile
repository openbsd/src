#	$OpenBSD: Makefile,v 1.21 2013/11/06 19:37:06 miod Exp $

STRIP?= strip

S=	${.CURDIR}/../../../..
DEFS= -DSUN_BOOTPARAMS
#-DNETIF_DEBUG
INCPATH=-I${.CURDIR} -I${.CURDIR}/../libsa -I${.CURDIR}/../libbug \
	-I${S} -I${S}/lib/libsa
CFLAGS+=${DEFS} ${INCPATH} ${COPTS}
CLEANFILES+=netboot netboot.bin

.include "${S}/arch/mvme88k/stand/bugcrt/Makefile.inc"
.include "${S}/arch/mvme88k/stand/libbug/Makefile.inc"
.include "${S}/arch/mvme88k/stand/libsa/Makefile.inc"
.include "${S}/arch/mvme88k/stand/libz/Makefile.inc"

SRCS=	boot.c devices.c version.c
SRCS+=	conf.c dev_net.c devopen.c netif.c
SRCS+=	if_ie.c
SRCS+=	if_le.c
OBJS=  ${SRCS:S/.c/.o/g}
LIBS= ${LIBSA} ${LIBBUG} ${LIBZ}
LDFLAGS=-N -T ${S}/arch/mvme88k/stand/ld.script -Ttext ${STAGE2_RELOC}
LDFLAGS+= -warn-common
NOPIE=

all:	netboot.bin

netboot: ${NETBOOT_CRT} ${OBJS} ${LIBS}
	${LD} ${LDFLAGS} -o $@ \
	    ${NETBOOT_CRT} ${OBJS} ${LIBS} `${CC} -print-libgcc-file-name`

netboot.bin: netboot
	${STRIP} netboot
	dd ibs=32 skip=1 if=netboot of=$@

install:
	${INSTALL} ${INSTALL_COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	  netboot.bin ${DESTDIR}${MDEC_DIR}/netboot

.include <bsd.prog.mk>
