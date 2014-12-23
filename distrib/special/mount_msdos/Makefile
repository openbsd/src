#	$OpenBSD: Makefile,v 1.1 2014/12/23 17:16:02 deraadt Exp $

PROG=	mount_msdos
SRCS=	mount_msdos.c getmntopts.c
MAN=	mount_msdos.8

MOUNT=	${.CURDIR}/../../../sbin/mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.PATH:  ${.CURDIR}/../../../sbin/mount_msdos
.include <bsd.prog.mk>
