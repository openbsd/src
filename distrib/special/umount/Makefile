#	$OpenBSD: Makefile,v 1.1 2014/02/21 19:01:02 deraadt Exp $

PROG=	umount
MAN=	umount.8

CFLAGS+= -DNO_NFS
DPADD+=	${LIBUTIL}
LDADD+=	-lutil

.PATH:	${.CURDIR}/../../../sbin/umount
.include <bsd.prog.mk>
