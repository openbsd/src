#	$OpenBSD: Makefile,v 1.1 1998/09/05 17:33:30 art Exp $

PROG=	mount_xfs
SRCS=	mount_xfs.c getmntopts.c
MAN=	mount_xfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${.CURDIR}/../../sys -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
