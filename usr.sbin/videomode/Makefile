#	$OpenBSD: Makefile,v 1.3 2000/02/12 15:24:56 espie Exp $

.if ${MACHINE} == "amiga"
PROG=videomode
CFLAGS+=-I${.CURDIR}/../../sys/arch -D${MACHINE}
.else
NOPROG=yes
.endif

NOMAN=	noman
MANSUBDIR=/amiga

.include <bsd.prog.mk>
