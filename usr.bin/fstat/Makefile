#	$OpenBSD: Makefile,v 1.10 2009/07/08 16:04:00 millert Exp $

PROG=	fstat
SRCS=	fstat.c fuser.c
MAN=	fstat.1 fuser.1
DPADD=	${LIBKVM}
LDADD=	-lkvm
LINKS=  ${BINDIR}/fstat ${BINDIR}/fuser

CFLAGS+=-DINET6

.include <bsd.prog.mk>

