#	$OpenBSD: Makefile,v 1.5 1997/09/21 11:36:37 deraadt Exp $

PROG=	fsck_ffs
MAN=	fsck_ffs.8
SRCS=	dir.c inode.c main.c pass1.c pass1b.c pass2.c pass3.c pass4.c \
	pass5.c fsutil.c setup.c utilities.c ffs_subr.c ffs_tables.c
.PATH:	${.CURDIR}/../../sys/ufs/ffs ${.CURDIR}/../fsck
CFLAGS+= -I${.CURDIR}/../fsck

.if make(install)
SUBDIR+= SMM.doc
.endif

.include <bsd.prog.mk>
