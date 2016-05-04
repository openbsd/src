#	$OpenBSD: Makefile,v 1.12 2016/05/04 19:48:08 jca Exp $

PROG=	fstat
SRCS=	fstat.c fuser.c
MAN=	fstat.1 fuser.1
DPADD=	${LIBKVM}
LDADD=	-lkvm
LINKS=  ${BINDIR}/fstat ${BINDIR}/fuser

.include <bsd.prog.mk>
