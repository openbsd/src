#	$OpenBSD: Makefile,v 1.2 2008/09/12 15:59:37 jmc Exp $

.if ${MACHINE} == "sparc64"

PROG=	mksuncd
CFLAGS+=-Werror -Wall -Wstrict-prototypes -Wmissing-prototypes

.else
NOPROG=
.endif

MAN=	mksuncd.1
MANSUBDIR=sparc64

.include <bsd.prog.mk>

