# $OpenBSD: Makefile,v 1.2 2001/06/22 16:12:18 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kdestroy

SRCS=	kdestroy.c

.PATH:	${KRB5SRC}/kuser

LDADD+=	-lkrb5 -ldes -lkafs -lkrb -lasn1 -lcrypto ${LIBROKEN}

.include <bsd.prog.mk>