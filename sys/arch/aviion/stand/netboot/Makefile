#	$OpenBSD: Makefile,v 1.1 2006/05/16 22:48:18 miod Exp $

SIZE?=	size

S=	${.CURDIR}/../../../..
DEFS=	-DSUN_BOOTPARAMS 
#DEFS+=	-DNETIF_DEBUG
INCPATH=-I${.CURDIR} -I${.CURDIR}/../libsa -I${.CURDIR}/../libprom \
	-I${S} -I${S}/lib/libsa
CFLAGS+=${DEFS} ${INCPATH} ${COPTS}
CLEANFILES+=netboot netboot.aout

.include "${S}/arch/aviion/stand/Makefile.inc"
.include "${S}/arch/aviion/stand/libprom/Makefile.inc"
.include "${S}/arch/aviion/stand/libsa/Makefile.inc"
.include "${S}/arch/aviion/stand/libz/Makefile.inc"

# Keep crt.c first in the object list for ld's sake.
SRCS=   crt.c boot.c conf.c devopen.c dev_net.c if_le.c version.c
OBJS=  ${SRCS:S/.c/.o/g}
LIBS=	${LIBSA} ${LIBPROM} ${LIBZ}
LDFLAGS+= -N -T ${STAGE2_RELOC} 

all:	netboot.aout

netboot: netboot.aout a2coff
	${DESTDIR}${MDEC_DIR}/a2coff netboot.aout netboot

netboot.aout: ${OBJS} ${SINGLE} ${LIBS}
	${LD} ${LDFLAGS} -o $@ \
		${SINGLE} ${OBJS} ${LIBS} `cc -print-libgcc-file-name`
	@${SIZE} $@

a2coff:	${DESTDIR}${MDEC_DIR}/a2coff
	cd ${S}/arch/aviion/stand/a2coff && ${MAKE} && ${MAKE} install

install: netboot
	${INSTALL} ${INSTALL_COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	  netboot ${DESTDIR}${MDEC_DIR}/netboot

.include <bsd.prog.mk>
