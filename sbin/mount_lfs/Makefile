#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:37:19 deraadt Exp $

PROG=	mount_lfs
SRCS=	mount_lfs.c getmntopts.c
MAN=	mount_lfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
