#	$OpenBSD: Makefile,v 1.4 2002/02/19 19:51:54 miod Exp $

.if ${MACHINE}=="hp300"
PROG=	hilinfo
CFLAGS+= -I${.CURDIR}/../../sys/arch/hp300
.else
NOPROG=yes
.endif

MAN=hilinfo.8
MANSUBDIR=hp300

.include <bsd.prog.mk>
