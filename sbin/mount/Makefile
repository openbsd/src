#	$OpenBSD: Makefile,v 1.6 2004/05/09 22:51:52 deraadt Exp $

PROG=	mount
SRCS=	mount.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	mount.8
# We do NOT install the getmntopts.3 man page.

.include <bsd.prog.mk>
