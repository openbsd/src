#	$OpenBSD: Makefile,v 1.3 1996/06/23 14:30:48 deraadt Exp $

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
