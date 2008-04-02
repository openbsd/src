#	$OpenBSD: Makefile,v 1.15 2008/04/02 21:53:18 miod Exp $

SIZE?=	size
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

SRCS=   boot.c conf.c version.c devopen.c dev_net.c
SRCS+=	if_ie.c
#SRCS+=  if_le.c
OBJS=  ${SRCS:S/.c/.o/g}
LIBS= ${LIBSA} ${LIBBUG} ${LIBZ}
LDFLAGS+= -s -N -T ${STAGE2_RELOC}

all:	netboot.bin

netboot: ${OBJS} ${NETBOOT_CRT} ${LIBS}
	${LD} ${LDFLAGS} -o $@ \
		${NETBOOT_CRT} ${OBJS} ${LIBS} `cc -print-libgcc-file-name`
#	@${SIZE} $@

netboot.bin: netboot
	${STRIP} netboot
	dd ibs=32 skip=1 if=netboot of=$@

install:
	${INSTALL} ${INSTALL_COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	  netboot.bin ${DESTDIR}${MDEC_DIR}/netboot

.include <bsd.prog.mk>
