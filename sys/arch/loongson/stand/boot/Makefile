#	$OpenBSD: Makefile,v 1.2 2010/02/17 19:51:30 miod Exp $

NOMAN=
#MAN=		boot.8
#MLINKS=	boot.8 boot.conf.5
.if ${MACHINE} == "loongson"
PROG=		boot
.include	"${.CURDIR}/../boot/Makefile.inc"
.else
NOPROG=
.endif

.include <bsd.prog.mk>
