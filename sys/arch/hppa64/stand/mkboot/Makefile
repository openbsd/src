#	$OpenBSD: Makefile,v 1.1 2005/04/01 10:40:49 mickey Exp $

MAN=	mkboot.8
MANSUBDIR=hppa64

.if ${MACHINE} == "hppa64"
PROG=	mkboot
CC=	${HOSTCC}
LDSTATIC=	-static
.else
NOPROG=
.endif

.include <bsd.prog.mk>
