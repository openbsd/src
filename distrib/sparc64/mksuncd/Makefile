#	$OpenBSD: Makefile,v 1.3 2003/03/02 12:32:08 deraadt Exp $

.if ${MACHINE} == "sparc64"

PROG=mksuncd
BINDIR=/usr/bin

CFLAGS+=-Werror -Wall -Wstrict-prototypes -Wmissing-prototypes

.else
NOPROG=
.endif

.include <bsd.prog.mk>

