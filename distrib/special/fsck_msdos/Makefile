#	$OpenBSD: Makefile,v 1.1 2014/12/23 17:16:02 deraadt Exp $

PROG=	fsck_msdos
MAN=	fsck_msdos.8
SRCS=	main.c check.c boot.c fat.c dir.c fsutil.c
.PATH: ${.CURDIR}/../../../sbin/fsck
CFLAGS+= -I${.CURDIR}/../../../sbin/fsck
DPADD+=	${LIBUTIL}
LDADD+=	-lutil

.PATH:  ${.CURDIR}/../../../sbin/fsck_msdos
.include <bsd.prog.mk>
