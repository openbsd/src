#	$OpenBSD: Makefile,v 1.3 1998/10/28 18:08:50 art Exp $

PROG=	mount_xfs
SRCS=	mount_xfs.c getmntopts.c
MAN=	mount_xfs.8
LINKS=	${BINDIR}/mount_xfs
MLINKS=	mount_xfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${.CURDIR}/../../sys -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
