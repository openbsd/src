#	$OpenBSD: Makefile,v 1.3 2012/09/12 09:19:54 haesbaert Exp $

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

