# $OpenBSD: Makefile,v 1.3 2002/05/11 00:20:19 espie Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kfd
MAN=	${PROG}.8

SRCS=	kfd.c

.PATH:	${KRB5SRC}/appl/kf
CFLAGS+=-I${KRB5SRC}/lib/krb5
DPADD+=	${LIBHDB} ${LIBKRB5} ${LIBKRB} ${LIBDES} ${LIBASN1} ${LIBCRYPTO} \
	${LIBCOM_ERR} ${LIBKAFS}
LDADD+=	-lhdb -lkrb5 -lkrb -ldes ${LIBROKEN} -lasn1 -lcrypto -lcom_err -lkafs

.include <bsd.prog.mk>
