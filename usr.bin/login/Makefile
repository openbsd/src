#	$OpenBSD: Makefile,v 1.19 2001/06/13 20:37:24 millert Exp $

PROG=	login
SRCS=	login.c failedlogin.c
DPADD=	${LIBUTIL}
LDADD=	-lutil

.if (${MACHINE_ARCH} == "alpha")
LDSTATIC=	${STATIC}
.endif

BINOWN=	root
BINMODE=4555

.include <bsd.prog.mk>
