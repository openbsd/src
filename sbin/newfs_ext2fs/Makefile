# $OpenBSD: Makefile,v 1.2 2010/02/17 19:26:48 otto Exp $
PROG=	newfs_ext2fs
SRCS=	newfs_ext2fs.c mke2fs.c ext2fs_bswap.c
MAN=	newfs_ext2fs.8

DPADD+= ${LIBUTIL}
LDADD+= -lutil

.include <bsd.prog.mk>
