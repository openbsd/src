#	$OpenBSD: Makefile,v 1.1 2014/12/23 17:16:02 deraadt Exp $

PROG=	fsck
SRCS=	fsck.c fsutil.c preen.c
MAN=	fsck.8
DPADD=	${LIBUTIL}
LDADD=	-lutil

.PATH:  ${.CURDIR}/../../../sbin/fsck
.include <bsd.prog.mk>
