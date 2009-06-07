#	$OpenBSD: Makefile,v 1.9 2009/06/07 03:10:09 millert Exp $

PROG=	fstat
DPADD=	${LIBKVM}
LDADD=	-lkvm

CFLAGS+=-DINET6

.include <bsd.prog.mk>

