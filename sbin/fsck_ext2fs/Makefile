#	$OpenBSD: Makefile,v 1.8 2001/09/18 13:31:29 art Exp $

PROG=	fsck_ext2fs
MAN=    fsck_ext2fs.8
SRCS=	dir.c inode.c main.c pass1.c pass1b.c pass2.c pass3.c pass4.c \
	pass5.c fsutil.c setup.c utilities.c ext2fs_bswap.c
.PATH:	${.CURDIR}/../../sys/ufs/ext2fs ${.CURDIR}/../fsck
CFLAGS+= -I${.CURDIR}/../fsck

.include <bsd.prog.mk>
