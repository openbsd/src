#	$OpenBSD: Makefile,v 1.18 2001/05/29 22:18:57 millert Exp $

PROG=	login
SRCS=	login.c failedlogin.c
DPADD=	${LIBUTIL}
LDADD=	-lutil

BINOWN=	root
BINMODE=4555

.include <bsd.prog.mk>
