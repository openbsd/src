#	$OpenBSD: Makefile,v 1.2 2001/01/29 02:05:46 niklas Exp $
#	$NetBSD: Makefile,v 1.2 1995/04/20 22:40:40 cgd Exp $

PROG=	sigsetjmptest
SRCS=	jmptest.c
NOMAN=	noman, no way, man

CFLAGS+= -DTEST_SIGSETJMP

.PATH:	${.CURDIR}/../setjmp

install:

regress: ${PROG}
	./${PROG} save
	./${PROG} nosave

.include <bsd.prog.mk>
