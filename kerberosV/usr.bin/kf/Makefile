# $OpenBSD: Makefile,v 1.1 2001/06/24 05:45:48 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kf

SRCS=	kf.c

.PATH:	${KRB5SRC}/appl/kf

LDADD+=	-lkrb5 -ldes -lkafs -l45 -lkrb ${LIBROKEN} -lasn1 -lcrypto -lcom_err

.include <bsd.prog.mk>