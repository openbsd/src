#	$OpenBSD: Makefile,v 1.8 2011/07/06 18:32:59 miod Exp $
#	$NetBSD: Makefile,v 1.12 2002/02/24 01:04:25 matt Exp $

S=	${.CURDIR}/../../../../

PROG=	xxboot
LINKS=	${BINDIR}/xxboot ${BINDIR}/raboot 
LINKS+=	${BINDIR}/xxboot ${BINDIR}/hdboot
LINKS+=	${BINDIR}/xxboot ${BINDIR}/sdboot
WARNS?=	1

SRCS=	start.S bootxx.c romread.S str.S urem.s udiv.s

INSTALL_STRIP=
CPPFLAGS+=-D_STANDALONE -DLIBSA_NO_FD_CHECKING -DLIBSA_NO_RAW_ACCESS \
	-DLIBSA_NO_TWIDDLE -DLIBSA_SINGLE_DEVICE=rom \
	-DLIBSA_NO_COMPAT_UFS \
	-DLIBSA_NO_FS_SYMLINK -DLIBSA_NO_FS_CLOSE \
	-DLIBSA_NO_FS_WRITE -DLIBSA_NO_FS_SEEK \
	-DNEED_UFS
BINDIR=	/usr/mdec
NOMAN=	# defined

CFLAGS=	-Os

SAREL=
SA_AS=	library
.include "${S}/lib/libsa/Makefile.inc"
LIBSA=	${SALIB}

.if ${MACHINE} == "vax"
.PHONY: machine-links
beforedepend: machine-links
machine-links:
	@[ -h machine ] || ln -s ${S}/arch/${MACHINE}/include machine
	@[ -h ${MACHINE_ARCH} ] || ln -s ${S}/arch/${MACHINE_ARCH}/include ${MACHINE_ARCH}
.NOPATH: machine ${MACHINE_ARCH}
CLEANFILES+= machine ${MACHINE_ARCH} ${PROG}.out
.endif

${PROG}: ${OBJS} ${LIBSA}
	${LD} -N -Ttext 100000 -o ${PROG}.out ${OBJS} ${LIBSA}
	/usr/bin/strip ${PROG}.out
	/usr/bin/size ${PROG}.out
	/bin/dd if=${PROG}.out of=${PROG} bs=32 skip=1

clean::
	rm -f a.out [Ee]rrs mklog core *.core ${PROG} ${OBJS} ${LOBJS} \
	    ${CLEANFILES} 

.include <bsd.prog.mk>
