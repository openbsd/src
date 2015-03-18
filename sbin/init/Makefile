#	$OpenBSD: Makefile,v 1.8 2015/03/18 19:55:52 pascal Exp $

PROG=	init
MAN=	init.8
DPADD=	${LIBUTIL}
LDADD=	-lutil
BINMODE=500
INSTALLFLAGS=-fschg
CFLAGS+=-DDEBUGSHELL -DSECURE -DLOGIN_CAP

.include <bsd.prog.mk>
