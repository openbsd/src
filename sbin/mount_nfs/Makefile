#	$OpenBSD: Makefile,v 1.9 2004/05/18 10:54:08 otto Exp $

PROG=	mount_nfs
SRCS=	mount_nfs.c getmntopts.c
MAN=	mount_nfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -DNFS -I${MOUNT}
.PATH:	${MOUNT}
CFLAGS+=-DNFS

.include <bsd.prog.mk>
