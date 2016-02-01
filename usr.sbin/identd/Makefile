#	$OpenBSD: Makefile,v 1.2 2016/02/01 19:08:33 jca Exp $

PROG=	identd
LDADD=	-levent
DPADD=	${LIBEVENT}
CFLAGS+= -Wall -Werror

MAN=	identd.8

.include <bsd.prog.mk>
