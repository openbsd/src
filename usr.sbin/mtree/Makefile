#	$OpenBSD: Makefile,v 1.3 1996/12/10 08:25:57 deraadt Exp $
#	$NetBSD: Makefile,v 1.8 1995/03/07 21:12:04 cgd Exp $

PROG=	mtree
#CFLAGS+=-DDEBUG
CFLAGS+=-Wall
MAN=	mtree.8
SRCS=	compare.c crc.c create.c misc.c mtree.c spec.c verify.c
.PATH:	${.CURDIR}/../../usr.bin/cksum

.include <bsd.prog.mk>
