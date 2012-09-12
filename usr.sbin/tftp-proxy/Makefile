#	$OpenBSD: Makefile,v 1.4 2012/09/12 09:19:54 haesbaert Exp $

PROG=	tftp-proxy
SRCS=	tftp-proxy.c filter.c
MAN=	tftp-proxy.8
LDADD=	-levent
DPADD=	${LIBEVENT}

CFLAGS+=-Wall -Werror
CDIAGFLAGS=

.include <bsd.prog.mk>
