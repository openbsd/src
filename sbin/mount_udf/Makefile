#	$OpenBSD: Makefile,v 1.1 2005/03/29 17:54:52 pedro Exp $

PROG=	mount_udf
SRCS=	mount_udf.c getmntopts.c
MAN=

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
