#	$OpenBSD: Makefile,v 1.8 2008/03/30 19:54:05 miod Exp $
#	$NetBSD: Makefile,v 1.27 2002/04/07 07:00:25 matt Exp $

S!=	cd ${.CURDIR}/../../../../; pwd
NOMAN=	# defined

.include <bsd.own.mk>

PROG=	boot
DEVS=	hp.c ctu.c ra.c mfm.c if_qe.c if_le.c if_ze.c if_de.c if_ni.c
SRCS=	srt0.S boot.c devopen.c conf.c autoconf.c netio.c rom.c romread.S \
	urem.s udiv.s consio.c consio2.S str.S ${DEVS} findcpu.c

CLEANFILES+=${PROG} ${PROG}.sym ${PROG}.mop
CPPFLAGS+=-DSUPPORT_BOOTPARAMS -DSUPPORT_DHCP -D_STANDALONE \
	-DNO_MID_CHECK
#CPPFLAGS+=-DBOOTP_DEBUG -DNETIF_DEBUG -DETHER_DEBUG -DNFS_DEBUG -DDEV_DEBUG \
#	-DRPC_DEBUG -DRARP_DEBUG -DPARANOID

BINDIR=/usr/mdec

SA_ZLIB=	yes
USE_LOADFILE=	yes
SAREL=
.include "${S}/lib/libsa/Makefile.inc"
LIBSA=	${SALIB}

Z_AS=	library
CFLAGS+=-I${S}/lib/libsa 
.include "${S}/lib/libz/Makefile.inc"
LIBZ=	${ZLIB}

.PATH:	${S}/lib/libkern/arch/vax ${S}/lib/libkern
SRCS+= moddi3.c negdi2.c

.if ${MACHINE} == "vax"
.PHONY: machine-links
beforedepend: machine-links
machine-links:
	@rm -f machine && ln -s ${S}/arch/${MACHINE}/include machine
	@rm -f ${MACHINE_ARCH} && ln -s ${S}/arch/${MACHINE_ARCH}/include ${MACHINE_ARCH}
.NOPATH: machine ${MACHINE_ARCH}
CLEANFILES+= machine ${MACHINE_ARCH}
CPPFLAGS+=	-I${.OBJDIR}
.endif

START=nisse

${PROG}: machine-links ${OBJS} ${LIBSA} ${LIBZ}
	${LD} -N -Ttext ${RELOC} -e ${START} -o ${PROG}.sym -Llib/sa -L. ${OBJS} \
	    ${LIBSA} ${LIBZ} ${LIBSA}
	/usr/sbin/mopa.out ${PROG}.sym ${PROG}.mop
	/bin/cp ${PROG}.sym ${PROG}
	/usr/bin/strip ${PROG}
	/usr/bin/size ${PROG}

clean::	
	rm -f a.out [Ee]rrs mklog core *.core ${PROG} ${OBJS} ${LOBJS} \
	    ${CLEANFILES}

#install:
#	${INSTALL_FILE} -o ${BINOWN} -g ${BINGRP} -m 555 \
#	    ${PROG} ${DESTDIR}${MDEC_DIR}/${PROG}

.include <bsd.prog.mk>
