#	$OpenBSD: Makefile,v 1.6 2004/10/11 14:45:33 avsm Exp $

.if (${MACHINE} == "i386" || ${MACHINE} == "amd64")
PROG=	mount_ntfs
SRCS=	mount_ntfs.c getmntopts.c
.else
NOPROG=
.endif

MAN=	mount_ntfs.8
MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
