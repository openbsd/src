#	$Id: Makefile,v 1.1 2003/05/20 03:18:23 tedu Exp $

PROG=	mount_ntfs
SRCS=	mount_ntfs.c getmntopts.c
MAN=	mount_ntfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
