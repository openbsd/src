#	$OpenBSD: Makefile,v 1.5 1997/02/22 08:38:09 millert Exp $

STATIC=	-static

PROG=	init
MAN=	init.8
DPADD=	${LIBUTIL} 
LDADD=	-lutil 
BINMODE=500
INSTALLFLAGS=-fschg
CFLAGS+=-DDEBUGSHELL -DSECURE

.include <bsd.prog.mk>
