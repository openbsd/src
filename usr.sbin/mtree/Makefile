#	$OpenBSD: Makefile,v 1.4 1996/12/10 08:26:53 deraadt Exp $
#	$NetBSD: Makefile,v 1.8 1995/03/07 21:12:04 cgd Exp $

PROG=	mtree
#CFLAGS+=-DDEBUG
MAN=	mtree.8
SRCS=	compare.c crc.c create.c misc.c mtree.c spec.c verify.c
.PATH:	${.CURDIR}/../../usr.bin/cksum

.include <bsd.prog.mk>
