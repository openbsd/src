# $OpenBSD: Makefile,v 1.9 2002/12/02 09:00:22 miod Exp $
# $NetBSD: Makefile,v 1.5 1996/09/27 03:23:13 chopps Exp $

.if ${MACHINE}=="amiga"
PROG=loadkmap
BINDIR=/usr/sbin
SUBDIR=din es fr sw us
CFLAGS+=	-fno-stack-protector
.else
NOPROG=Yes
.endif

MAN=loadkmap.8
MANSUBDIR=/amiga
.include <bsd.prog.mk>
