#	$OpenBSD: Makefile,v 1.4 2002/02/18 11:35:09 art Exp $

PROG=	sigsetjmptest
SRCS=	jmptest.c

CFLAGS+= -DTEST_SIGSETJMP

.PATH:	${.CURDIR}/../setjmp

REGRESSTARGETS=sigsetjmp-save sigsetjmp-nosave

sigsetjmp-save: ${PROG}
	./${PROG} save
sigsetjmp-nosave: ${PROG}
	./${PROG} nosave

.PHONY: ${REGRESSTARGETS}

.include <bsd.regress.mk>
