#	$OpenBSD: Makefile,v 1.5 1997/09/21 11:37:03 deraadt Exp $

PROG=	mount
SRCS=	mount.c getmntopts.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	mount.8
# We do NOT install the getmntopts.3 man page.

.include <bsd.prog.mk>
