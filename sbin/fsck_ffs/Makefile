#	$OpenBSD: Makefile,v 1.2 1996/06/23 14:30:25 deraadt Exp $
#	$NetBSD: Makefile,v 1.10 1995/03/18 14:55:39 cgd Exp $

PROG=	fsck
MAN=	fsck.8
SRCS=	dir.c inode.c main.c pass1.c pass1b.c pass2.c pass3.c pass4.c \
	pass5.c preen.c setup.c utilities.c ffs_subr.c ffs_tables.c
.PATH:	${.CURDIR}/../../sys/ufs/ffs

.if make(install)
SUBDIR+= SMM.doc
.endif

.include <bsd.prog.mk>
