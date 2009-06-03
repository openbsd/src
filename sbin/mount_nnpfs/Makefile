#	$OpenBSD: Makefile,v 1.1 2009/06/03 14:45:49 jj Exp $

PROG=	mount_nnpfs
SRCS=	mount_nnpfs.c getmntopts.c
MAN=	mount_nnpfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${.CURDIR}/../../sys -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
