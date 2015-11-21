#	$OpenBSD: Makefile,v 1.7 2015/11/21 19:35:19 jmc Exp $

PROG=	newfs
SRCS=	dkcksum.c getmntopts.c newfs.c mkfs.c
MAN=	newfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+=-DMFS -I${MOUNT}
.PATH:	${MOUNT} ${.CURDIR}/../disklabel

DPADD=	${LIBUTIL}
LDADD=	-lutil

LINKS=	${BINDIR}/newfs ${BINDIR}/mount_mfs
MLINKS=	newfs.8 mount_mfs.8

.include <bsd.prog.mk>
