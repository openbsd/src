#	$NetBSD: Makefile,v 1.3 1995/03/18 14:57:12 cgd Exp $
#	@(#)Makefile	8.3 (Berkeley) 3/27/94

PROG=	mount_cd9660
SRCS=	mount_cd9660.c getmntopts.c
MAN=	mount_cd9660.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
