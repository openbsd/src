#	$OpenBSD: Makefile,v 1.7 2007/10/17 02:30:23 deraadt Exp $

MAN=pctr.1

CFLAGS+=-W -Wall -Werror

.if ${MACHINE} == "amd64" || ${MACHINE} == "i386"
PROG=	pctr
.else
NOPROG=
.endif

.include <bsd.prog.mk>
