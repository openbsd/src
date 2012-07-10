# $OpenBSD: Makefile,v 1.2 2012/07/10 10:28:05 nicm Exp $

PROG=	cu
SRCS=	cu.c command.c input.c error.c

CDIAGFLAGS+= -Wall -W -Wno-unused-parameter

LDADD=  -levent
DPADD=  ${LIBEVENT}

.include <bsd.prog.mk>
