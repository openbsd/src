#	$OpenBSD: Makefile,v 1.12 2003/10/01 22:50:29 miod Exp $

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
#LDFLAGS+= -nostdlib -s -N -Ttext ${RELOC}

all:	netboot.bin

netboot: ${OBJS} ${SINGLE} ${LIBS}
	${LD} ${LDFLAGS} -o $@ \
		${SINGLE} ${OBJS} ${LIBS}
#	@${SIZE} $@

netboot.bin: netboot
	${STRIP} netboot
	dd ibs=32 skip=1 if=netboot of=$@
#	dd ibs=38 skip=1 if=netboot of=$@

install:
	${INSTALL} ${INSTALL_COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	  netboot.bin ${DESTDIR}${MDEC_DIR}/netboot

.include <bsd.prog.mk>
