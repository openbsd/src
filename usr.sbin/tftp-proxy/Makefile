#	$OpenBSD: Makefile,v 1.1.1.1 2012/07/11 11:43:27 dlg Exp $

PROG=	tftp-proxy
SRCS=	tftp-proxy.c filter.c
MAN=	tftp-proxy.8
LDADD=	-levent
DPADD=	${LIBEVENT}

CFLAGS+=-Wall -Werror

.include <bsd.prog.mk>
