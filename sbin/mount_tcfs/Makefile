#	$OpenBSD: Makefile,v 1.2 2000/06/21 05:28:25 fgsch Exp $

PROG=	mount_tcfs
SRCS=	mount_tcfs.c getmntopts.c
MAN=	mount_tcfs.8

MOUNT=	${.CURDIR}/../mount_nfs
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
