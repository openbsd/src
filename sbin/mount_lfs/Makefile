#	$OpenBSD: Makefile,v 1.2 1996/06/23 14:31:23 deraadt Exp $
#	$NetBSD: Makefile,v 1.3 1995/03/18 14:57:29 cgd Exp $

PROG=	mount_lfs
SRCS=	mount_lfs.c getmntopts.c
MAN=	mount_lfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
