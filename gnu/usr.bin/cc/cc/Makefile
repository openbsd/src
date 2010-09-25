# $FreeBSD: src/gnu/usr.bin/cc/cc/Makefile,v 1.33.8.1 2009/04/15 03:14:26 kensmith Exp $
# $OpenBSD: Makefile,v 1.2 2010/09/25 17:44:34 drahn Exp $

.include <bsd.own.mk>

.include "${.CURDIR}/../Makefile.inc"
.include "${.CURDIR}/../Makefile.fe"

PROG=	cc
MAN=	gcc.1
SRCS+=  gccspec.c

SPECDIR= /usr/lib/gcc-lib/${GCC_TARGET}/${BASEVER}

LINKS=	${BINDIR}/cc ${BINDIR}/gcc
MLINKS=	gcc.1 cc.1 gcc.1 c++.1 gcc.1 g++.1 gcc.1 c++filt.1

beforeinstall:
.if !exist(${DESTDIR}${SPECDIR})
	${INSTALL} -d -o ${BINOWN} -g ${BINGRP} -m ${DIRMODE} \
		${DESTDIR}${SPECDIR}
.endif

afterinstall:
	${.OBJDIR}/cc -dumpspecs > ${DESTDIR}${SPECDIR}/specs

.include <bsd.prog.mk>
