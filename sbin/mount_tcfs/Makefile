#	$OpenBSD: Makefile,v 1.1 2000/06/18 00:25:07 provos Exp $

PROG=	mount_tcfs
SRCS=	mount_tcfs.c getmntopts.c
MAN=	mount_tcfs.8

MOUNT=	${.CURDIR}/../mount_nfs
CFLAGS+= -I${.CURDIR}/../../sys -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
