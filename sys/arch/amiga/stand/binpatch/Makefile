#	$OpenBSD: Makefile,v 1.5 2000/02/12 15:29:05 espie Exp $
#	$NetBSD: Makefile,v 1.6 1996/09/27 03:23:12 chopps Exp $

.if ${MACHINE} == "amiga"
PROG=binpatch
BINDIR=/sbin
LDFLAGS+=-static
.else
NOPROG=Yes
.endif

MAN=binpatch.8
MANSUBDIR=/amiga

.include <bsd.prog.mk>
