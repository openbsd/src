#	$OpenBSD: Makefile,v 1.7 2000/09/03 18:41:16 espie Exp $

PROG=	mount_nfs
SRCS=	mount_nfs.c getmntopts.c
MAN=	mount_nfs.8

# Temporarily removed to use own getmntops.c - fvdl
#
#MOUNT=	${.CURDIR}/../mount
#CFLAGS+= -DNFS -I${MOUNT}
#.PATH:	${MOUNT}
CFLAGS+=-DNFS

# NFSKERB is not currently supported
#
#.include <bsd.own.mk>	# For KERBEROS
#
#.if (${KERBEROS:L} == "yes")
#CFLAGS+=-DNFSKERB
#DPADD+=	${LIBKRB} ${LIBDES}
#LDADD+=	-lkrb -ldes
#.endif

.include <bsd.prog.mk>
