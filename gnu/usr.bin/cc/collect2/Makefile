# $FreeBSD: src/gnu/usr.bin/cc/collect2/Makefile,v 1.9.20.1 2009/04/15 03:14:26 kensmith Exp $
# $OpenBSD: Makefile,v 1.2 2010/05/06 20:58:10 naddy Exp $

.include <bsd.own.mk>

.include "${.CURDIR}/../Makefile.inc"
.include "${.CURDIR}/../Makefile.ver"

.PATH: ${GCCDIR}

PROG=	collect2
SRCS=	collect2.c tlink.c version.c
NOMAN=	Yes
BINDIR= /usr/lib/gcc-lib/${GCC_TARGET}/${BASEVER}

LDADD= ${LIBIBERTY}

.include <bsd.prog.mk>
