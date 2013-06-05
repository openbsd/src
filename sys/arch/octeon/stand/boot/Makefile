#	$OpenBSD: Makefile,v 1.1 2013/06/05 01:02:29 jasper Exp $

NOMAN=
#MAN=		boot.8
#MLINKS=	boot.8 boot.conf.5

.if ${MACHINE} == "octeon"
PROG=		boot
.include	"${.CURDIR}/Makefile.inc"
.else
NOPROG=
.endif

CFLAGS+=-fno-pie
AFLAGS+=-fno-pie
LDFLAGS+=-nopie

.include <bsd.prog.mk>
