# $OpenBSD: Makefile,v 1.7 2001/09/27 04:55:16 art Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kadmind
MAN=	${PROG}.8

SRCS=	kadmind.c server.c version4.c kadm_conn.c

.PATH:	${KRB5SRC}/kadmin
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
LDADD+=	-lhdb -lkadm5srv -lkrb5 -lkrb -ldes ${LIBROKEN} -lasn1 -lcrypto \
	-lcom_err -lkafs

.include <bsd.prog.mk>

.if (${ELF_TOOLCHAIN:L} == "yes")
LDADD+= -ldl
.endif
