# $OpenBSD: Makefile,v 1.4 2003/05/17 01:31:21 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kfd
MAN=	${PROG}.8

SRCS=	kfd.c

.PATH:	${KRB5SRC}/appl/kf
CFLAGS+=-I${KRB5SRC}/lib/krb5
DPADD+=	${LIBHDB} ${LIBKRB5} ${LIBKRB} ${LIBDES} ${LIBASN1} ${LIBCRYPTO} \
	${LIBCOM_ERR} ${LIBKAFS}
LDADD+=	-lhdb -lkrb5 -ldes ${LIBROKEN} -lasn1 -lcrypto -lcom_err

.include <bsd.prog.mk>
