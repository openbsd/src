#	$OpenBSD: Makefile,v 1.4 2004/01/24 21:10:55 miod Exp $

SIZE?=	size
STRIP?= strip

S=	${.CURDIR}/../../../..
DEFS= -DSUN_BOOTPARAMS 
#-DNETIF_DEBUG
INCPATH=-I${.CURDIR} -I${.CURDIR}/../libsa -I${.CURDIR}/../libbug \
	-I${S} -I${S}/lib/libsa
CFLAGS=-O2 ${DEFS} ${INCPATH} ${COPTS}
CLEANFILES+=tftpboot tftpboot.bin

.include "${S}/arch/mvmeppc/stand/bugcrt/Makefile.inc"
.include "${S}/arch/mvmeppc/stand/libbug/Makefile.inc"
.include "${S}/arch/mvmeppc/stand/libsa/Makefile.inc"
.include "${S}/arch/mvmeppc/stand/libz/Makefile.inc"

SRCS=   boot.c version.c
#SRCS+=	if_ie.c 
#SRCS+=  if_le.c
OBJS=  ${SRCS:S/.c/.o/g}
LIBS= ${LIBSA} ${LIBBUG} ${LIBZ}
LDFLAGS+= -s -X -N -Ttext ${STAGE1_RELOC} -e _start 
#LDFLAGS+= -nostdlib -s -N -Ttext ${RELOC}

all:	tftpboot.bin

tftpboot: ${OBJS} ${SINGLE} ${LIBS}
	${LD} ${LDFLAGS} \
		${SINGLE} ${OBJS} ${LIBS} -o $@
#	@${SIZE} $@

tftpboot.bin: tftpboot
	${STRIP} tftpboot
	dd ibs=96 skip=1 if=tftpboot of=$@
#	dd ibs=38 skip=1 if=tftpboot of=$@

install:
	${INSTALL} ${INSTALL_COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	  tftpboot.bin ${DESTDIR}${MDEC_DIR}/tftpboot

.include <bsd.prog.mk>
