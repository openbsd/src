# $OpenBSD: Makefile,v 1.1 2001/05/25 08:20:34 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	klist

SRCS=	klist.c

.PATH:	${KRB5SRC}/kuser

CFLAGS+=-I${KRB5SRC}/lib/roken
LDADD+=	-lkrb5 -ldes -lkafs -lkrb ${LIBROKEN} 

.include <bsd.prog.mk>