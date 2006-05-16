#	$OpenBSD: Makefile,v 1.3 2006/05/16 22:51:30 miod Exp $

SIZE?=	size
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
LDFLAGS+= -s -N -T ${STAGE2_RELOC}

all:	tftpboot.bin

tftpboot: ${OBJS} ${SINGLE} ${LIBS}
	${LD} ${LDFLAGS} -o $@ \
		${SINGLE} ${OBJS} ${LIBS} `cc -print-libgcc-file-name`
#	@${SIZE} $@

tftpboot.bin: tftpboot
	${STRIP} tftpboot
	dd ibs=32 skip=1 if=tftpboot of=$@
#	dd ibs=38 skip=1 if=tftpboot of=$@

install:
	${INSTALL} ${INSTALL_COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	  tftpboot.bin ${DESTDIR}${MDEC_DIR}/tftpboot

.include <bsd.prog.mk>
