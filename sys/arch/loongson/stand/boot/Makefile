#	$OpenBSD: Makefile,v 1.3 2012/08/31 16:13:59 pascal Exp $

NOMAN=
#MAN=		boot.8
#MLINKS=	boot.8 boot.conf.5
.if ${MACHINE} == "loongson"
PROG=		boot
.include	"${.CURDIR}/../boot/Makefile.inc"
.else
NOPROG=
.endif
CFLAGS+=-fno-pie
AFLAGS+=-fno-pie
LDFLAGS+=-nopie

.include <bsd.prog.mk>
