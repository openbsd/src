#	$OpenBSD: Makefile,v 1.6 2000/08/20 18:42:39 millert Exp $

STATIC=	-static

PROG=	init
MAN=	init.8
DPADD=	${LIBUTIL} 
LDADD=	-lutil 
BINMODE=500
INSTALLFLAGS=-fschg
CFLAGS+=-DDEBUGSHELL -DSECURE -DLOGIN_CAP

.include <bsd.prog.mk>
