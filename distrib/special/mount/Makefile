#	$OpenBSD: Makefile,v 1.1 2014/12/23 17:16:02 deraadt Exp $

PROG=	mount
SRCS=	mount.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	mount.8

.PATH:	${.CURDIR}/../../../sbin/mount
.include <bsd.prog.mk>
