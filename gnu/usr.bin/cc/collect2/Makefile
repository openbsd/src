# $FreeBSD: src/gnu/usr.bin/cc/collect2/Makefile,v 1.9.20.1 2009/04/15 03:14:26 kensmith Exp $
# $OpenBSD: Makefile,v 1.3 2010/09/25 17:44:34 drahn Exp $

.include <bsd.own.mk>

.include "${.CURDIR}/../Makefile.inc"
.include "${.CURDIR}/../Makefile.ver"

.PATH: ${GCCDIR}

PROG=	collect2
SRCS=	collect2.c tlink.c version.c
NOMAN=	Yes
BINDIR= /usr/lib/gcc-lib/${GCC_TARGET}/${BASEVER}

LDADD= ${LIBIBERTY}

.if defined(CROSSDIR)
CFLAGS+=  -DTARGET_MACHINE=\"$(GCC_TARGET)\"
.endif

.include <bsd.prog.mk>
