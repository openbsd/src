#	$OpenBSD: Makefile,v 1.4 2016/07/30 03:25:49 guenther Exp $

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
LDFLAGS+=-nopie -znorelro

.include <bsd.prog.mk>
