# $OpenBSD: Makefile,v 1.3 2001/06/28 14:24:04 todd Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kadmind
MAN=	${PROG}.8

SRCS=	kadmind.c server.c version4.c kadm_conn.c

.PATH:	${KRB5SRC}/kadmin
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
LDADD+=	-lhdb -lkadm5srv -lkrb5 -lkrb -ldes ${LIBROKEN} -lasn1 -lcrypto \
	-lcom_err

.include <bsd.prog.mk>

.if defined(ELF_TOOLCHAIN)
.if (${ELF_TOOLCHAIN:L} == "yes")
LDADD+= -ldl
.endif
.endif
