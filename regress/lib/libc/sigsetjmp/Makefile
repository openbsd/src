#	$OpenBSD: Makefile,v 1.3 2002/01/01 23:00:52 art Exp $
#	$NetBSD: Makefile,v 1.2 1995/04/20 22:40:40 cgd Exp $

PROG=	sigsetjmptest
SRCS=	jmptest.c

CFLAGS+= -DTEST_SIGSETJMP

.PATH:	${.CURDIR}/../setjmp

REGRESSTARGETS=sigsetjmp-save sigsetjmp-nosave

sigsetjmp-save: ${PROG}
	./${PROG} save
sigsetjmp-nosave: ${PROG}
	./${PROG} nosave

.include <bsd.regress.mk>
