#	$NetBSD: Makefile,v 1.14 1997/02/21 07:47:49 mikel Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/5/93

PROG=	fsck_ext2fs
MAN=    fsck_ext2fs.8
SRCS=	dir.c inode.c main.c pass1.c pass1b.c pass2.c pass3.c pass4.c \
	pass5.c fsutil.c setup.c utilities.c# ext2fs_subr.c
.PATH:	${.CURDIR}/../../sys/ufs/ext2fs ${.CURDIR}/../fsck
#CFLAGS= -g -I${.CURDIR}/../fsck
CFLAGS+= -g -I${.CURDIR}/../fsck

.include <bsd.prog.mk>
