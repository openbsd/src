#	$OpenBSD: Makefile,v 1.1 2014/12/23 17:16:02 deraadt Exp $

PROG=	mount_ffs
SRCS=	mount_ffs.c getmntopts.c
MAN=	mount_ffs.8

MOUNT=	${.CURDIR}/../../../sbin/mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.PATH:  ${.CURDIR}/../../../sbin/mount_ffs
.include <bsd.prog.mk>
