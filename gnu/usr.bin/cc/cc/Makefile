# $FreeBSD: src/gnu/usr.bin/cc/cc/Makefile,v 1.33.8.1 2009/04/15 03:14:26 kensmith Exp $
# $OpenBSD: Makefile,v 1.8 2017/07/24 19:22:49 robert Exp $

.include <bsd.own.mk>

.include "${.CURDIR}/../Makefile.inc"
.include "${.CURDIR}/../Makefile.fe"

PROG=	cc
MAN=	gcc.1
SRCS+=  gccspec.c

SPECDIR= /usr/lib/gcc-lib/${GCC_TARGET}/${BASEVER}

.if ${COMPILER_VERSION:L} == "gcc4"
LINKS=	${BINDIR}/gcc ${BINDIR}/cc
.endif

beforeinstall:
.for dir in /usr/lib/gcc-lib /usr/lib/gcc-lib/${GCC_TARGET} ${SPECDIR}
	${INSTALL} -d -o ${BINOWN} -g ${BINGRP} -m ${DIRMODE} ${DESTDIR}${dir}
.endfor

realinstall:
	${INSTALL} ${INSTALL_COPY} -S ${INSTALL_STRIP} \
	    -o ${BINOWN} -g ${BINGRP} \
	    -m ${BINMODE} ${PROG} ${DESTDIR}${BINDIR}/g${PROG}

afterinstall:
	${.OBJDIR}/cc -dumpspecs > ${DESTDIR}${SPECDIR}/specs
	chown ${BINOWN}:${BINGRP} ${DESTDIR}${SPECDIR}/specs
	chmod ${NONBINMODE} ${DESTDIR}${SPECDIR}/specs

.include <bsd.prog.mk>
