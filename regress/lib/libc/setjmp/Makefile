#	$OpenBSD: Makefile,v 1.2 2001/01/29 02:05:45 niklas Exp $
#	$NetBSD: Makefile,v 1.2 1995/04/20 22:40:13 cgd Exp $

PROG=	setjmptest
SRCS=	jmptest.c
NOMAN=	noman, no way, man

CFLAGS+= -DTEST_SETJMP

.PATH:	${.CURDIR}/../setjmp

install:

regress: ${PROG}
	./${PROG}

.include <bsd.prog.mk>
