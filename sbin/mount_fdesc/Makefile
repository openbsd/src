#	$OpenBSD: Makefile,v 1.2 1996/06/23 14:31:17 deraadt Exp $
#	$NetBSD: Makefile,v 1.6 1995/03/18 14:57:17 cgd Exp $

PROG=	mount_fdesc
SRCS=	mount_fdesc.c getmntopts.c
MAN=	mount_fdesc.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
