#	$NetBSD: Makefile,v 1.4 1995/10/08 23:44:02 gwr Exp $

MAN=	installboot.8
BINDIR=/usr/mdec
MANSUBDIR=/sun3

.if ${MACHINE} == "sun3"
PROG=	installboot

# Need this to work in the miniroot
LDSTATIC=	-static
.else
NOPROG=
.endif

.include <bsd.prog.mk>
