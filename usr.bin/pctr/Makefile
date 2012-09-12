#	$OpenBSD: Makefile,v 1.8 2012/09/12 09:19:54 haesbaert Exp $

MAN=pctr.1

CFLAGS+=-W -Wall -Werror
CDIAGFLAGS=

.if ${MACHINE} == "amd64" || ${MACHINE} == "i386"
PROG=	pctr
.else
NOPROG=
.endif

.include <bsd.prog.mk>
