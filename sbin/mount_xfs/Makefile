#	$OpenBSD: Makefile,v 1.2 1998/09/05 20:00:05 art Exp $

PROG=	mount_xfs
SRCS=	mount_xfs.c getmntopts.c
MAN=	mount_xfs.8
LINKS=	${BINDIR}/mount_xfs ${BINDIR}/mount_afs
MLINKS=	mount_xfs.8 mount_afs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${.CURDIR}/../../sys -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
