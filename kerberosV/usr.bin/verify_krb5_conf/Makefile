# $OpenBSD: Makefile,v 1.1 2001/06/23 21:54:09 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	verify_krb5_conf

.PATH:	${KRB5SRC}/lib/krb5
MAN=	${PROG}.8

CFLAGS+=-I${KRB5SRC}/lib/roken
LDADD+=	-lkrb5 -ldes -lkafs -lkrb ${LIBROKEN} -lasn1 -lcrypto -lcom_err

.include <bsd.prog.mk>