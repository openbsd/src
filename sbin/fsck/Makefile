#	$OpenBSD: Makefile,v 1.4 1996/12/04 01:31:10 deraadt Exp $
#	$NetBSD: Makefile,v 1.14 1996/09/27 22:38:37 christos Exp $

PROG=	fsck
SRCS=	fsck.c fsutil.c preen.c
MAN=	fsck.8
DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
