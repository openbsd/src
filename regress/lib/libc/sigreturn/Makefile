# $OpenBSD: Makefile,v 1.1 2001/12/17 02:36:00 marc Exp $

PROG=	sigret
SRCS=	sigret.c
NOMAN=	noman

DEBUG+= -ggdb

.PATH:	${.CURDIR}/../sigret

install:

regress: ${PROG}
	./${PROG}

.include <bsd.prog.mk>
