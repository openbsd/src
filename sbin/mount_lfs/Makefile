#	$NetBSD: Makefile,v 1.3 1995/03/18 14:57:29 cgd Exp $
#	@(#)Makefile	8.2 (Berkeley) 3/27/94

PROG=	mount_lfs
SRCS=	mount_lfs.c getmntopts.c
MAN=	mount_lfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
