#	$OpenBSD: Makefile,v 1.4 2012/11/04 16:30:44 matthieu Exp $

NOMAN=
#MAN=		boot.8
#MLINKS=	boot.8 boot.conf.5
.if ${MACHINE} == "loongson"
PROG=		boot
.include	"${.CURDIR}/Makefile.inc"
.else
NOPROG=
.endif
CFLAGS+=-fno-pie
AFLAGS+=-fno-pie
LDFLAGS+=-nopie

.include <bsd.prog.mk>
