# $FreeBSD: src/gnu/usr.bin/cc/c++/Makefile,v 1.24.8.1 2009/04/15 03:14:26 kensmith Exp $
# $OpenBSD: Makefile,v 1.2 2017/07/24 19:22:49 robert Exp $

.include <bsd.own.mk>

.include "${.CURDIR}/../Makefile.inc"
.include "${.CURDIR}/../Makefile.fe"

.PATH: ${GCCDIR}/cp

PROG=	c++
SRCS+=	g++spec.c

.if ${COMPILER_VERSION:L} == "gcc4"
LINKS=	${BINDIR}/g++ ${BINDIR}/c++
.endif

NOMAN=	Yes

DPADD=	${LIBCPP} ${LIBIBERTY}
LDADD=	${LIBCPP} ${LIBIBERTY}

realinstall:
	${INSTALL} ${INSTALL_COPY} -S ${INSTALL_STRIP} \
	    -o ${BINOWN} -g ${BINGRP} \
	    -m ${BINMODE} ${PROG} ${DESTDIR}${BINDIR}/${PROG:S/c/g/g}

.include <bsd.prog.mk>
