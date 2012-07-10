# $OpenBSD: Makefile,v 1.1 2012/07/10 08:02:27 nicm Exp $

PROG=	cu
SRCS=	cu.c command.c input.c

CDIAGFLAGS+= -Wall -W -Wno-unused-parameter

LDADD=  -levent
DPADD=  ${LIBEVENT}

.include <bsd.prog.mk>
