#	$OpenBSD: Makefile,v 1.2 1996/06/26 05:31:15 deraadt Exp $
#	$NetBSD: Makefile,v 1.7 1995/03/25 06:39:33 glass Exp $

PROG=	ar
CFLAGS+=-I${.CURDIR}
SRCS=	append.c ar.c archive.c contents.c delete.c extract.c misc.c \
	move.c print.c replace.c 
MAN=	ar.1 ar.5

.include <bsd.prog.mk>
