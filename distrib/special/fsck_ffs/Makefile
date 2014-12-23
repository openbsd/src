#	$OpenBSD: Makefile,v 1.1 2014/12/23 17:16:02 deraadt Exp $

PROG=	fsck_ffs
MAN=	fsck_ffs.8
SRCS=	dir.c inode.c main.c pass1.c pass1b.c pass2.c pass3.c pass4.c \
	pass5.c fsutil.c setup.c utilities.c ffs_subr.c ffs_tables.c
CFLAGS+= -I${.CURDIR}/../../../sbin/fsck
DPADD+=	${LIBUTIL}
LDADD+=	-lutil

.PATH:  ${.CURDIR}/../../../sbin/fsck_ffs
.PATH:	${.CURDIR}/../../../sbin/fsck
.PATH:	${.CURDIR}/../../../sys/ufs/ffs 
.include <bsd.prog.mk>
