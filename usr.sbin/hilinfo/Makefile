#	$OpenBSD: Makefile,v 1.3 2000/05/18 16:46:38 espie Exp $

.if ${MACHINE}=="hp300"
PROG=	hilinfo
CFLAGS+= -I${.CURDIR}/../../sys/arch/hp300
.else
NOPROG=yes
.endif

NOMAN=
MANSUBDIR=hp300

.include <bsd.prog.mk>
