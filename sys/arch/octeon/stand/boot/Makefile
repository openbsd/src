#	$OpenBSD: Makefile,v 1.5 2016/11/02 19:07:39 jasper Exp $

NOMAN=
#MAN=		boot.8

.if ${MACHINE} == "octeon"
PROG=		boot
.include	"${.CURDIR}/Makefile.inc"
.else
NOPROG=
.endif

CFLAGS+=-fno-pie -DCONSPEED=115200
AFLAGS+=-fno-pie
LDFLAGS+=-nopie -znorelro

.include <bsd.prog.mk>
