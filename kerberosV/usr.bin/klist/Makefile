# $OpenBSD: Makefile,v 1.2 2001/06/22 16:12:19 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	klist

SRCS=	klist.c

.PATH:	${KRB5SRC}/kuser

CFLAGS+=-I${KRB5SRC}/lib/roken
LDADD+=	-lkrb5 -ldes -lkafs -lkrb ${LIBROKEN} -lasn1 -lcrypto

.include <bsd.prog.mk>