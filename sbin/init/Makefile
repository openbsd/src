#	$OpenBSD: Makefile,v 1.9 2015/09/25 16:09:09 tedu Exp $

PROG=	init
MAN=	init.8
DPADD=	${LIBUTIL}
LDADD=	-lutil
CFLAGS+=-DDEBUGSHELL -DSECURE -DLOGIN_CAP

.include <bsd.prog.mk>
