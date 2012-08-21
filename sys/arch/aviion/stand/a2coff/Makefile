#	$OpenBSD: Makefile,v 1.2 2012/08/21 14:46:18 pascal Exp $

MAN=	a2coff.8
MANSUBDIR=aviion
.if ${MACHINE} == "aviion"
PROG=	a2coff
BINDIR=/usr/mdec
NOPIE=
.else
NOPROG=
.endif

.include <bsd.prog.mk>
