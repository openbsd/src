#	$OpenBSD: Makefile,v 1.12 2013/10/15 22:49:03 deraadt Exp $
#	$NetBSD: Makefile,v 1.27 2002/04/07 07:00:25 matt Exp $

S!=	cd ${.CURDIR}/../../../../; pwd
NOMAN=	# defined
NOPIE=

.include <bsd.own.mk>

PROG=	boot
DEVS=	ra.c mfm.c if_qe.c if_le.c if_ze.c if_de.c
SRCS=	srt0.S boot.c devopen.c conf.c autoconf.c netio.c rom.c romread.S \
	urem.s udiv.s consio.c consio2.S str.S ${DEVS} findcpu.c

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
.if !make(obj)
.BEGIN:
	@([ -h machine ] || ln -s ${.CURDIR}/../../../${MACHINE}/include machine)
.NOPATH: machine
CLEANFILES+=	machine
.endif
.endif

START=start
STRIP?=strip
CLEANFILES+=	${PROG}.sym

${PROG}: ${OBJS} ${LIBSA} ${LIBZ}
	${LD} -N -Ttext ${RELOC} -e ${START} -o ${PROG}.sym ${OBJS} \
	    -Llib/sa -L. ${LIBSA} ${LIBZ} ${LIBSA}
	${STRIP} -o ${PROG} ${PROG}.sym

clean::
	rm -f a.out [Ee]rrs mklog *.core ${PROG} ${OBJS} ${LOBJS} \
	    ${CLEANFILES}

.include <bsd.prog.mk>
