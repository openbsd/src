#	$OpenBSD: Makefile,v 1.1 2014/12/23 17:16:02 deraadt Exp $

PROG=	fsck_ext2fs
MAN=    fsck_ext2fs.8
SRCS=	dir.c inode.c main.c pass1.c pass1b.c pass2.c pass3.c pass4.c \
	pass5.c fsutil.c setup.c utilities.c ext2fs_bswap.c
CFLAGS+= -I${.CURDIR}/../../../sbin/fsck

.PATH:	${.CURDIR}/../../../sbin/fsck_ext2fs
.PATH:	${.CURDIR}/../../../sbin/fsck
.PATH:	${.CURDIR}/../../../sys/ufs/ext2fs
.include <bsd.prog.mk>
