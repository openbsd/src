#	$OpenBSD: Makefile,v 1.2 1996/06/23 14:31:26 deraadt Exp $
#	$NetBSD: Makefile,v 1.9 1995/03/18 14:57:35 cgd Exp $

PROG=	mount_msdos
SRCS=	mount_msdos.c getmntopts.c
MAN=	mount_msdos.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
