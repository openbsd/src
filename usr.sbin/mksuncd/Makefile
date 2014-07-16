#	$OpenBSD: Makefile,v 1.4 2014/07/16 20:07:03 okan Exp $

.if ${MACHINE} == "sparc64"

PROG=	mksuncd
CFLAGS+=-Werror -Wall -Wstrict-prototypes -Wmissing-prototypes
CDIAGFLAGS=

.else
NOPROG=
.endif

MAN=	mksuncd.1
MANSUBDIR=sparc64

.include <bsd.prog.mk>
