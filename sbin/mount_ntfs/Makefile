#	$OpenBSD: Makefile,v 1.3 2003/05/20 05:05:17 tedu Exp $

.if (${MACHINE} == "i386")
PROG=	mount_ntfs
SRCS=	mount_ntfs.c getmntopts.c
MAN=	mount_ntfs.8
.else
NOPROG=
.endif

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
