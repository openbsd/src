# $FreeBSD: src/gnu/usr.bin/cc/cc/Makefile,v 1.33.8.1 2009/04/15 03:14:26 kensmith Exp $
# $OpenBSD: Makefile,v 1.6 2016/09/27 19:30:01 natano Exp $

.include <bsd.own.mk>

.include "${.CURDIR}/../Makefile.inc"
.include "${.CURDIR}/../Makefile.fe"

PROG=	cc
MAN=	gcc.1
SRCS+=  gccspec.c

SPECDIR= /usr/lib/gcc-lib/${GCC_TARGET}/${BASEVER}

LINKS=	${BINDIR}/cc ${BINDIR}/gcc

beforeinstall:
.for dir in /usr/lib/gcc-lib /usr/lib/gcc-lib/${GCC_TARGET} ${SPECDIR}
	${INSTALL} -d -o ${BINOWN} -g ${BINGRP} -m ${DIRMODE} ${DESTDIR}${dir}
.endfor

afterinstall:
	${.OBJDIR}/cc -dumpspecs > ${DESTDIR}${SPECDIR}/specs
	chown ${BINOWN}:${BINGRP} ${DESTDIR}${SPECDIR}/specs

.include <bsd.prog.mk>
