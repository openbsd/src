#	$OpenBSD: Makefile,v 1.3 1996/06/23 14:31:50 deraadt Exp $
#	$NetBSD: Makefile,v 1.6 1996/05/16 07:17:49 thorpej Exp $

PROG=	newlfs
SRCS=	dkcksum.c lfs.c lfs_cksum.c misc.c newfs.c
MAN=	newlfs.8
.PATH:	${.CURDIR}/../../sys/ufs/lfs ${.CURDIR}/../disklabel

DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
