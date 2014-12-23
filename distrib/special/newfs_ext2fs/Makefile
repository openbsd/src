# $OpenBSD: Makefile,v 1.1 2014/12/23 17:16:02 deraadt Exp $
PROG=	newfs_ext2fs
SRCS=	newfs_ext2fs.c mke2fs.c ext2fs_bswap.c
MAN=	newfs_ext2fs.8

.PATH:	${.CURDIR}/../../../sys/ufs/ext2fs
DPADD+= ${LIBUTIL}
LDADD+= -lutil

.PATH:  ${.CURDIR}/../../../sbin/newfs_ext2fs
.include <bsd.prog.mk>
