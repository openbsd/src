#	$OpenBSD: Makefile,v 1.17 2001/05/29 21:39:26 millert Exp $

PROG=	login
SRCS=	login.c failedlogin.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
CFLAGS+= -Wall -Wno-unused

BINOWN=	root
BINMODE=4555

.include <bsd.prog.mk>
