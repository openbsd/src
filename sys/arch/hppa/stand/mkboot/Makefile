#	$OpenBSD: Makefile,v 1.4 2002/11/12 00:47:43 deraadt Exp $

MAN=	mkboot.8
MANSUBDIR=/hppa

.if ${MACHINE} == "hppa"
PROG=	mkboot
CC=	${HOSTCC}
CFLAGS=-g -O
LDSTATIC=	-static
.else
NOPROG=
.endif

.include <bsd.prog.mk>
