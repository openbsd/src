#	$OpenBSD: Makefile,v 1.2 1996/06/23 14:31:39 deraadt Exp $
#	$NetBSD: Makefile,v 1.4 1995/03/18 14:58:14 cgd Exp $

PROG=	mount_umap
SRCS=	mount_umap.c getmntopts.c
MAN=	mount_umap.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${.CURDIR}/../../sys -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
