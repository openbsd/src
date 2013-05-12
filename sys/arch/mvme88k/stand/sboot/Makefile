#	$OpenBSD: Makefile,v 1.8 2013/05/12 10:43:45 miod Exp $

S=	${.CURDIR}/../../../..
DEFS=	-DSUN_BOOTPARAMS -D_STANDALONE
INCPATH=-I${.CURDIR} -I${.CURDIR}/../libsa -I${.CURDIR}/../libbug \
	-I${S} -I${S}/lib/libsa -I${.CURDIR}/../netboot
CFLAGS+=${DEFS} ${INCPATH} ${COPTS}

.include "${S}/arch/mvme88k/stand/bugcrt/Makefile.inc"
.include "${S}/arch/mvme88k/stand/libbug/Makefile.inc"
.include "${S}/arch/mvme88k/stand/libsa/Makefile.inc"
.include "${S}/arch/mvme88k/stand/libz/Makefile.inc"

OBJCOPY?=	objcopy

SRCS=   sboot.c devices.c devopen.c version.c

.PATH:	${S}/arch/mvme88k/stand/netboot

SRCS+=	conf.c dev_net.c netif.c
SRCS+=	if_ie.c
SRCS+=	if_le.c

OBJS=	${SRCS:S/.c/.o/g}
LIBS=	${LIBSA} ${LIBBUG} ${LIBZ}
LDFLAGS= -N -T ${S}/arch/mvme88k/stand/ld.script -Ttext ${STAGE1_RELOC}
LDFLAGS+= -warn-common
NOPIE=

CLEANFILES+=sboot.tmp sboot

all:	sboot

sboot:	${SREC_CRT} ${OBJS} ${LIBS}
	${LD} ${LDFLAGS} -Ttext ${STAGE1_RELOC} ${SREC_CRT} ${OBJS} -o $@.tmp \
	    ${LIBS} `${CC} -print-libgcc-file-name`
	${OBJCOPY} -S -O srec $@.tmp $@

install:
	${INSTALL} ${INSTALL_COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    sboot ${DESTDIR}${MDEC_DIR}/sboot

.include <bsd.prog.mk>
