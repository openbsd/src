#	$OpenBSD: Makefile,v 1.3 1996/11/12 08:46:12 downsj Exp $
#	$NetBSD: Makefile,v 1.10 1995/07/12 03:45:09 cgd Exp $

PROG=	mount
SRCS=	mount.c getmntopts.c readlabel.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	mount.8
# We do NOT install the getmntopts.3 man page.

.include <bsd.prog.mk>
