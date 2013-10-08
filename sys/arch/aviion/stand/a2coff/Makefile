#	$OpenBSD: Makefile,v 1.3 2013/10/08 21:49:40 miod Exp $

MAN=	a2coff.8
MANSUBDIR=aviion
.if ${MACHINE} == "aviion"
PROG=	a2coff
BINDIR=/usr/mdec
NOPIE=
HOSTCFLAGS?=	-O2 ${PIPE} ${DEBUG}
.else
NOPROG=
.endif

.include <bsd.prog.mk>
CFLAGS=	${HOSTCFLAGS}
