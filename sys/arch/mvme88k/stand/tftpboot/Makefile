#	$OpenBSD: Makefile,v 1.1 2004/01/26 19:48:34 miod Exp $

SIZE?=	size
STRIP?= strip

S=	${.CURDIR}/../../../..
DEFS= -DSUN_BOOTPARAMS 
#-DNETIF_DEBUG
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
LDFLAGS+= -s -N -T ${STAGE2_RELOC} 
#LDFLAGS+= -nostdlib -s -N -Ttext ${RELOC}

all:	tftpboot.bin

tftpboot: ${OBJS} ${SINGLE} ${LIBS}
	${LD} ${LDFLAGS} -o $@ \
		${SINGLE} ${OBJS} ${LIBS}
#	@${SIZE} $@

tftpboot.bin: tftpboot
	${STRIP} tftpboot
	dd ibs=32 skip=1 if=tftpboot of=$@
#	dd ibs=38 skip=1 if=tftpboot of=$@

install:
	${INSTALL} ${INSTALL_COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	  tftpboot.bin ${DESTDIR}${MDEC_DIR}/tftpboot

.include <bsd.prog.mk>
