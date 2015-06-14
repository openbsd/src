#	$OpenBSD: Makefile,v 1.7 2015/06/14 19:37:11 miod Exp $

SIZE?=	size

S=	${.CURDIR}/../../../..
DEFS=	-DSUN_BOOTPARAMS
#DEFS+=	-DNETIF_DEBUG
INCPATH=-I${.CURDIR} -I${.CURDIR}/../libsa -I${.CURDIR}/../libprom \
	-I${S} -I${S}/lib/libsa
CFLAGS+=${DEFS} ${INCPATH} ${COPTS} -DSTAGE1_RELOC=${STAGE1_RELOC}
CLEANFILES+=boot boot.ELF

.include "${S}/arch/aviion/stand/Makefile.inc"
.include "${S}/arch/aviion/stand/libprom/Makefile.inc"
.include "${S}/arch/aviion/stand/libsa/Makefile.inc"
.include "${S}/arch/aviion/stand/libz/Makefile.inc"

# Keep crt.c first in the object list for ld's sake.
SRCS=	crt.c boot.c conf.c devopen.c version.c
SRCS+=	oaic.c oosiop.c scsi.c sd.c
SRCS+=	dev_net.c if_le.c
.PATH:	${S}/lib/libkern
SRCS+=	moddi3.c muldi3.c negdi2.c qdivrem.c
OBJS=  ${SRCS:S/.c/.o/g}
LIBS=	${LIBSA} ${LIBPROM} ${LIBZ}
LDFLAGS+= -N -Ttext ${STAGE2_RELOC} -e start
NOPIE=

all:	boot

boot: boot.ELF
	${MDEC_DIR}/a2coff boot.ELF boot

boot.ELF: ${OBJS} ${LIBS}
	${LD} ${LDFLAGS} -o $@ ${OBJS} ${LIBS}
	@${SIZE} $@

install: boot
	${INSTALL} ${INSTALL_COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	  boot ${DESTDIR}${MDEC_DIR}/boot

.include <bsd.prog.mk>
