#	$OpenBSD: Makefile,v 1.7 2002/06/09 08:13:06 todd Exp $

STATIC=	-static

PROG=	init
MAN=	init.8
DPADD=	${LIBUTIL}
LDADD=	-lutil
BINMODE=500
INSTALLFLAGS=-fschg
CFLAGS+=-DDEBUGSHELL -DSECURE -DLOGIN_CAP

.include <bsd.prog.mk>
