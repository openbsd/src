#	$OpenBSD: Makefile,v 1.4 1996/12/04 08:39:31 deraadt Exp $
#	$NetBSD: Makefile,v 1.6 1996/05/16 07:17:49 thorpej Exp $

PROG=	newfs_lfs
SRCS=	dkcksum.c lfs.c lfs_cksum.c misc.c newfs.c
MAN=	newlfs.8
.PATH:	${.CURDIR}/../../sys/ufs/lfs ${.CURDIR}/../disklabel

DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
