#	$OpenBSD: Makefile,v 1.9 2017/04/16 10:16:35 jsg Exp $

MAN=pctr.1

CFLAGS+=-W -Wall
CDIAGFLAGS=

.if ${MACHINE} == "amd64" || ${MACHINE} == "i386"
PROG=	pctr
.else
NOPROG=
.endif

.include <bsd.prog.mk>
