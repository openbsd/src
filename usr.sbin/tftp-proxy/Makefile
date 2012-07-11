#	$OpenBSD: Makefile,v 1.2 2012/07/11 11:45:37 dlg Exp $

PROG=	tftp-proxy
SRCS=	tftp-proxy.c filter.c
MAN=	#tftp-proxy.8
LDADD=	-levent
DPADD=	${LIBEVENT}

CFLAGS+=-Wall -Werror

.include <bsd.prog.mk>
