#	$OpenBSD: Makefile,v 1.2 1996/06/23 14:31:15 deraadt Exp $
#	$NetBSD: Makefile,v 1.3 1995/03/18 14:57:12 cgd Exp $

PROG=	mount_cd9660
SRCS=	mount_cd9660.c getmntopts.c
MAN=	mount_cd9660.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
