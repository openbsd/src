#	$NetBSD: Makefile,v 1.4 1995/03/18 14:58:14 cgd Exp $
#	@(#)Makefile	8.3 (Berkeley) 3/27/94

PROG=	mount_umap
SRCS=	mount_umap.c getmntopts.c
MAN=	mount_umap.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${.CURDIR}/../../sys -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
