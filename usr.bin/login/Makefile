#	$OpenBSD: Makefile,v 1.20 2001/06/25 05:51:25 art Exp $

PROG=	login
SRCS=	login.c failedlogin.c
DPADD=	${LIBUTIL}
LDADD=	-lutil

BINOWN=	root
BINMODE=4555

.include <bsd.prog.mk>
