# $OpenBSD: Makefile,v 1.1 2001/05/25 08:20:33 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kdestroy

SRCS=	kdestroy.c

.PATH:	${KRB5SRC}/kuser

LDADD+=	${LIBROKEN} -lkrb5 -ldes -lkafs -lkrb

.include <bsd.prog.mk>