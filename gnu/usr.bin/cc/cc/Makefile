# $FreeBSD: src/gnu/usr.bin/cc/cc/Makefile,v 1.33.8.1 2009/04/15 03:14:26 kensmith Exp $
# $OpenBSD: Makefile,v 1.5 2016/03/30 06:38:40 jmc Exp $

.include <bsd.own.mk>

.include "${.CURDIR}/../Makefile.inc"
.include "${.CURDIR}/../Makefile.fe"

PROG=	cc
MAN=	gcc.1
SRCS+=  gccspec.c

SPECDIR= /usr/lib/gcc-lib/${GCC_TARGET}/${BASEVER}

LINKS=	${BINDIR}/cc ${BINDIR}/gcc

beforeinstall:
.if !exists(${DESTDIR}${SPECDIR})
	${INSTALL} -d -o ${BINOWN} -g ${BINGRP} -m ${DIRMODE} \
		${DESTDIR}${SPECDIR}
.endif

afterinstall:
	${.OBJDIR}/cc -dumpspecs > ${DESTDIR}${SPECDIR}/specs

.include <bsd.prog.mk>
