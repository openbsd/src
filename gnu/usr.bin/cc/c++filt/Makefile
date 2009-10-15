# $FreeBSD: src/gnu/usr.bin/cc/c++filt/Makefile,v 1.22.8.1 2009/04/15 03:14:26 kensmith Exp $
# $OpenBSD: Makefile,v 1.1.1.1 2009/10/15 20:50:19 robert Exp $

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

beforeinstall:
	${INSTALL} -d -o ${BINOWN} -g ${BINGRP} -m ${DIRMODE} \
		${DESTDIR}${BINDIR}

.include <bsd.prog.mk>
