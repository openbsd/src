#	$OpenBSD: Makefile,v 1.4 1996/04/21 23:46:48 deraadt Exp $
#	$NetBSD: Makefile,v 1.5 1996/02/18 11:59:09 fvdl Exp $
#	@(#)Makefile	8.2 (Berkeley) 3/27/94

PROG=	mount_nfs
SRCS=	mount_nfs.c getmntopts.c
MAN=	mount_nfs.8

# Temporarily removed to use own getmntops.c - fvdl
#
#MOUNT=	${.CURDIR}/../mount
#CFLAGS+= -DNFS -I${MOUNT}
#.PATH:	${MOUNT}
CFLAGS+=-DNFS

#.include <bsd.own.mk>	# For KERBEROS

.if defined(KERBEROS)
CFLAGS+=-DNFSKERB
DPADD+=	${LIBKRB} ${LIBDES}
LDADD+=	-lkrb -ldes
.endif

.include <bsd.prog.mk>
