#	$OpenBSD: Makefile,v 1.2 2005/02/25 09:06:28 otto Exp $

PROG=copy
CFLAGS+= -Wall

.if !(${MACHINE} == "i386" || ${MACHINE} == "amd64")
REGRESS_SKIP=
.endif

.include <bsd.regress.mk>
