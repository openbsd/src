#	$OpenBSD: Makefile,v 1.7 2002/05/11 00:20:20 espie Exp $

PROG=	fsdb
MAN=	fsdb.8
SRCS=	fsdb.c fsdbutil.c fsutil.c \
	dir.c inode.c pass1.c pass1b.c pass2.c pass3.c pass4.c \
	pass5.c preen.c setup.c utilities.c ffs_subr.c ffs_tables.c
CFLAGS+= -I${.CURDIR}/../../sbin/fsck -I${.CURDIR}/../../sbin/fsck_ffs
LDADD+= -ledit -lcurses
DPADD+=	${LIBEDIT} ${LIBCURSES}
.PATH:	${.CURDIR}/../../sbin/fsck ${.CURDIR}/../../sbin/fsck_ffs ${.CURDIR}/../../sys/ufs/ffs

.include <bsd.prog.mk>
