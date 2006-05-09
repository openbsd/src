#	$OpenBSD: Makefile,v 1.1.1.1 2006/05/09 19:32:43 miod Exp $

MAN=	a2coff.8
MANSUBDIR=aviion
.if ${MACHINE} == "aviion"
PROG=	a2coff
BINDIR=/usr/mdec
.else
NOPROG=
.endif

.include <bsd.prog.mk>
