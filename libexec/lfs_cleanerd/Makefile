#	from: @(#)Makefile	8.1 (Berkeley) 6/5/93
#	$OpenBSD: Makefile,v 1.2 1996/07/01 11:04:54 downsj Exp $

PROG=	lfs_cleanerd
CFLAGS+= -g -I${.CURDIR} -DDIAGNOSTIC
MAN=	lfs_cleanerd.8
SRCS=	cleanerd.c lfs_cksum.c library.c misc.c print.c

.PATH:	${.CURDIR}/../../sys/ufs/lfs

.include <bsd.prog.mk>
