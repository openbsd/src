#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:37:06 deraadt Exp $

PROG=	mount_ados
SRCS=	mount_ados.c getmntopts.c
MAN=	mount_ados.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
