#	$OpenBSD: Makefile,v 1.2 2014/07/12 23:45:33 jasper Exp $

NOMAN=
#MAN=		boot.8
#MLINKS=	boot.8 boot.conf.5

.if ${MACHINE} == "octeon"
PROG=		boot
.include	"${.CURDIR}/Makefile.inc"
.else
NOPROG=
.endif

CFLAGS+=-fno-pie -DCONSPEED=15200
AFLAGS+=-fno-pie
LDFLAGS+=-nopie

.include <bsd.prog.mk>
