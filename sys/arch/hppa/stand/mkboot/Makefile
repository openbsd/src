#	$OpenBSD: Makefile,v 1.5 2003/01/30 05:36:18 mickey Exp $

MAN=	mkboot.8
MANSUBDIR=/hppa

.if ${MACHINE} == "hppa"
PROG=	mkboot
CC=	${HOSTCC}
LDSTATIC=	-static
.else
NOPROG=
.endif

.include <bsd.prog.mk>
