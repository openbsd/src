#	$OpenBSD: Makefile,v 1.4 2004/02/21 03:37:16 deraadt Exp $

.if (${MACHINE} == "i386")
PROG=	mount_ntfs
SRCS=	mount_ntfs.c getmntopts.c
NOMAN=
.else
MAN=	mount_ntfs.8
NOPROG=
.endif

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
