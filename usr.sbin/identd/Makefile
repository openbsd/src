#	$OpenBSD: Makefile,v 1.1 2013/03/18 00:34:48 dlg Exp $

PROG=	identd
SRCS=	identd.c
LDADD=	-levent
DPADD=	${LIBEVENT}
CFLAGS+= -Wall -Werror

MAN=	identd.8

.include <bsd.prog.mk>
