#	$OpenBSD: Makefile,v 1.6 2002/12/02 09:00:22 miod Exp $
#	$NetBSD: Makefile,v 1.6 1996/09/27 03:23:12 chopps Exp $

.if ${MACHINE} == "amiga"
PROG=binpatch
BINDIR=/sbin
LDFLAGS+=-static
CFLAGS+=	-fno-stack-protector
.else
NOPROG=Yes
.endif

MAN=binpatch.8
MANSUBDIR=/amiga

.include <bsd.prog.mk>
