#	$OpenBSD: Makefile,v 1.5 1997/09/21 11:37:44 deraadt Exp $

PROG=	newfs_lfs
SRCS=	dkcksum.c lfs.c lfs_cksum.c misc.c newfs.c
MAN=	newlfs.8
.PATH:	${.CURDIR}/../../sys/ufs/lfs ${.CURDIR}/../disklabel

DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
