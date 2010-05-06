# $FreeBSD: src/gnu/usr.bin/cc/c++filt/Makefile,v 1.22.8.1 2009/04/15 03:14:26 kensmith Exp $
# $OpenBSD: Makefile,v 1.2 2010/05/06 20:58:10 naddy Exp $

.include <bsd.own.mk>

.include "${.CURDIR}/../Makefile.inc"
.include "${.CURDIR}/../Makefile.fe"

.PATH: ${BSDSRCDIR}/gnu/lib/libiberty/src

PROG=	c++filt
SRCS=	cp-demangle.c
NOMAN=	Yes

CFLAGS+= -DSTANDALONE_DEMANGLER -DVERSION=\"$(GCC_VERSION)\"

DPADD=	${LIBIBERTY}
LDADD=	${LIBIBERTY}

.include <bsd.prog.mk>
