#	$OpenBSD: Makefile,v 1.1 2008/08/22 15:48:32 deraadt Exp $

.if ${MACHINE} == "sparc64"

PROG=	mksuncd
CFLAGS+=-Werror -Wall -Wstrict-prototypes -Wmissing-prototypes

.else
NOPROG=
.endif

MAN=	mksuncd.1

.include <bsd.prog.mk>

