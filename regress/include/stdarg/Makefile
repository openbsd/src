#	$OpenBSD: Makefile,v 1.2 2001/01/29 02:05:35 niklas Exp $
#	$NetBSD: Makefile,v 1.2 1995/04/20 22:38:00 cgd Exp $

PROG=	stdargtest
SRCS=	argtest.c
NOMAN=	noman, no way, man

CFLAGS+= -DSTDARG

.PATH:	${.CURDIR}/../stdarg

install:

regress: ${PROG}
	./${PROG}

.include <bsd.prog.mk>
