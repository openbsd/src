#	$OpenBSD: Makefile,v 1.1 1997/01/20 09:15:31 mickey Exp $

LKM=	unionfs
SRCS+=	union_lkm.c union_vfsops.c union_vnops.c union_subr.c
CFLAGS+=-I${.CURDIR}/../../sys/miscfs/union
NOMAN=	noman

.PATH:	${.CURDIR}/../../sys/miscfs/union

.include <bsd.lkm.mk>
