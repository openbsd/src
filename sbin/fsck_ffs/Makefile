#	$OpenBSD: Makefile,v 1.6 2010/01/04 17:50:38 deraadt Exp $

PROG=	fsck_ffs
MAN=	fsck_ffs.8
SRCS=	dir.c inode.c main.c pass1.c pass1b.c pass2.c pass3.c pass4.c \
	pass5.c fsutil.c setup.c utilities.c ffs_subr.c ffs_tables.c
.PATH:	${.CURDIR}/../../sys/ufs/ffs ${.CURDIR}/../fsck
CFLAGS+= -I${.CURDIR}/../fsck

.include <bsd.prog.mk>
