#	$OpenBSD: Makefile,v 1.2 1996/06/23 14:31:37 deraadt Exp $
#	$NetBSD: Makefile,v 1.6 1995/03/18 14:58:08 cgd Exp $

PROG=	mount_procfs
SRCS=	mount_procfs.c getmntopts.c
MAN=	mount_procfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
