#	$OpenBSD: Makefile,v 1.3 2002/02/14 20:45:31 deraadt Exp $
#	$NetBSD: Makefile,v 1.4 1999/05/23 21:58:19 ragge Exp $

S!=	cd ${.CURDIR}/../../../../; pwd

NOMAN=	1
PROG=	boot
DEVS=	hp.c ctu.c ra.c tmscp.c mfm.c if_qe.c if_le.c if_ze.c
SRCS=	srt0.s boot.c devopen.c conf.c autoconf.c netio.c rom.c romread.s \
	urem.s udiv.s consio.c str.s ${DEVS} findcpu.c
#OBJS=	autoconf.o boot.o conf.o consio.o ctu.o devopen.o findcpu.o hp.o \
#	if_le.o if_qe.o if_ze.o mfm.o netio.o ra.o rom.o romread.o srt0.o \
#	str.o tmscp.o udiv.o urem.o

CLEANFILES+=${PROG}.mop
CPPFLAGS+=-DSUPPORT_BOOTPARAMS -DSUPPORT_DHCP -D_STANDALONE 
#CPPFLAGS+=-DBOOTP_DEBUG -DNETIF_DEBUG -DETHER_DEBUG -DNFS_DEBUG -DDEV_DEBUG \
#	-DRPC_DEBUG -DRARP_DEBUG -DPARANOID -DSUPPORT_BOOTP
BINDIR=	/usr/mdec

SA_ZLIB=	yes
SAREL=
.include "${S}/lib/libsa/Makefile.inc"
LIBSA=	${SALIB}

Z_AS=	library
CFLAGS+=-I${S}/lib/libsa 
.include "${S}/lib/libz/Makefile.inc"
LIBZ=	${ZLIB}

#KERN_AS=library
#.include "${S}/lib/libkern/Makefile.inc"
#LIBKERN=${KERNLIB}

boot: ${OBJS} ${SALIB} ${LIBZ} ${LIBKERN}
	ld -N -Ttext ${RELOC} -e nisse -o ${PROG} -Llib/sa -L. ${OBJS} \
    ${LIBSA} ${LIBZ} -lsa ${LIBKERN}
	/usr/sbin/mopa.out ${PROG} ${PROG}.mop
	strip ${PROG}
	size ${PROG}

clean::	
	rm -f a.out [Ee]rrs mklog core *.core ${PROG} ${OBJS} ${LOBJS} \
		${CLEANFILES}

.include <bsd.prog.mk>
