#	$OpenBSD: Makefile,v 1.1 2014/12/23 17:16:02 deraadt Exp $

PROG=	ls
SRCS=	cmp.c ls.c main.c print.c util.c
DPADD= ${LIBUTIL}
LDADD= -lutil

.PATH:  ${.CURDIR}/../../../bin/ls
.include <bsd.prog.mk>
