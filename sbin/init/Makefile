#	$OpenBSD: Makefile,v 1.4 1996/09/16 10:51:31 downsj Exp $

STATIC=	-static

PROG=	init
MAN=	init.8
#DPADD=	${LIBUTIL} ${LIBCRYPT}
DPADD=	${LIBUTIL} 
#LDADD=	-lutil -lcrypt
LDADD=	-lutil 
BINMODE=500
INSTALLFLAGS=-fschg
CFLAGS+=-DDEBUGSHELL -DSECURE

.include <bsd.prog.mk>
