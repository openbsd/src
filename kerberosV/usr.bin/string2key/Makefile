# $OpenBSD: Makefile,v 1.1 2001/06/27 16:17:10 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	string2key
MAN=	${PROG}.8

.PATH:	${KRB5SRC}/kdc

CFLAGS+=-I${KRB5SRC}/kdc -I${KRB5SRC}/lib/krb5
LDADD+=	-lkrb5 -ldes -lkafs -lkrb ${LIBROKEN} -lasn1 -lcrypto -lcom_err

.include <bsd.prog.mk>