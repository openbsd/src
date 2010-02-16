# $OpenBSD: Makefile,v 1.1 2010/02/16 08:25:55 otto Exp $
PROG=	newfs_ext2fs
SRCS=	newfs_ext2fs.c mke2fs.c ext2fs_bswap.c
MAN=	newfs_ext2fs.8

DPADD+= ${LIBUTIL}
LDADD+= -lutil

.PATH:  ${.CURDIR}/../../sys/ufs/ext2fs

.include <bsd.prog.mk>
