# $OpenBSD: Makefile,v 1.3 2001/06/23 00:17:46 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kdestroy

SRCS=	kdestroy.c

.PATH:	${KRB5SRC}/kuser

LDADD+=	-lkrb5 -ldes -lkafs -lkrb -lasn1 -lcrypto ${LIBROKEN} -lcom_err

.include <bsd.prog.mk>