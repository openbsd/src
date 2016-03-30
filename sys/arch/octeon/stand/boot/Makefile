#	$OpenBSD: Makefile,v 1.3 2016/03/30 06:38:45 jmc Exp $

NOMAN=
#MAN=		boot.8

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
