#	$OpenBSD: Makefile,v 1.11 2014/07/16 20:06:27 okan Exp $

PROG=	fstat
SRCS=	fstat.c fuser.c
MAN=	fstat.1 fuser.1
DPADD=	${LIBKVM}
LDADD=	-lkvm
LINKS=  ${BINDIR}/fstat ${BINDIR}/fuser

CFLAGS+=-DINET6

.include <bsd.prog.mk>
