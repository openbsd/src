
#	$OpenBSD: Makefile,v 1.5 1998/12/15 06:32:35 smurph Exp $

RELOC=0xAF0000
SIZE?=	size
STRIP?= strip

S=	${.CURDIR}/../../../..
DEFS= -DSUN_BOOTPARAMS 
#-DNETIF_DEBUG
INCPATH=-I${.CURDIR} -I${.CURDIR}/../libsa -I${.CURDIR}/../libbug \
	-I${S} -I${S}/lib/libsa
CFLAGS=	-O2 ${DEFS} ${INCPATH} ${COPTS}
CLEANFILES+=netboot netboot.bin

.include "${S}/arch/mvme88k/stand/bugcrt/Makefile.inc"
.include "${S}/arch/mvme88k/stand/libbug/Makefile.inc"
.include "${S}/arch/mvme88k/stand/libsa/Makefile.inc"
.include "${S}/arch/mvme88k/stand/libz/Makefile.inc"

SRCS=   boot.c conf.c version.c devopen.c dev_net.c
SRCS+=	if_ie.c if_le.c
OBJS=  ${SRCS:S/.c/.o/g}
LIBS= ${LIBSA} ${LIBBUG} ${LIBZ} -lgcc -lc -lgcc
LDFLAGS+= -s -N -T ${RELOC} 
#LDFLAGS+= -nostdlib -s -N -Ttext ${RELOC}

all:	netboot.bin

netboot: ${OBJS} ${BUGCRT} ${LIBS}
	${LD} ${LDFLAGS} \
		${BUGCRT} ${OBJS} ${LIBS} -o $@
#	@${SIZE} $@

netboot.bin: netboot
	${STRIP} netboot
	dd ibs=32 skip=1 if=netboot of=$@
#	dd ibs=38 skip=1 if=netboot of=$@
	chown ${LIBOWN}.${LIBGRP} netboot.bin
	chmod ${LIBMODE} netboot.bin

install:
	${INSTALL} ${INSTALL_COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	  netboot.bin ${DESTDIR}${MDEC_DIR}/netboot

.include <bsd.prog.mk>
