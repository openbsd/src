#	$NetBSD: Makefile,v 1.6 1996/05/16 07:17:49 thorpej Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/18/93

PROG=	newlfs
SRCS=	dkcksum.c lfs.c lfs_cksum.c misc.c newfs.c
MAN=	newlfs.8
.PATH:	${.CURDIR}/../../sys/ufs/lfs ${.CURDIR}/../disklabel

DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
