#	$Id: Makefile,v 1.2 1995/11/07 08:50:55 deraadt Exp $

RELOC=0x3F0000

S=	${.CURDIR}/../../../..
DEFS= -DSTANDALONE -DSUN_BOOTPARAMS
INCPATH=-I${.CURDIR} -I${.CURDIR}/../libsa -I${S} -I${S}/lib/libsa
CFLAGS=	-O2 ${INCPATH} ${DEFS} ${COPTS}

.include "${S}/arch/${MACHINE}/stand/libsa/Makefile.inc"
.PATH: ${S}/arch/${MACHINE}/stand/libsa
SRTOBJ= SRT0.o SRT1.o

SRCS=   boot.c conf.c version.c dev_net.c
SRCS+=	if_ie.c if_le.c
OBJS=  ${SRTOBJ} ${SRCS:S/.c/.o/g}

all:	netboot.bin

netboot: ${OBJS} ${LIBSA}
	${LD} -s -N -T ${RELOC} -e start -o $@ ${OBJS} ${LIBSA}
	@size $@

netboot.bin: netboot
	dd ibs=32 skip=1 if=netboot of=$@

install:
	install ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	  netboot.bin ${DESTDIR}${MDEC_DIR}/netboot

.include <bsd.prog.mk>
