# $OpenBSD: Makefile,v 1.12 2003/07/14 16:14:44 mho Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kadmind
MAN=	${PROG}.8

SRCS=	kadmind.c server.c kadm_conn.c
SRCS+=	${PRINTVER}
SRCS+=	mini_inetd.c socket.c

.PATH:	${KRB5SRC}/kadmin
.PATH:	${ROKENSRC}

CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/kdc

DPADD+=	${LIBKADM5SRV} ${LIBKRB5} ${LIBDES} ${LIBCRYPTO} ${LIBUTIL}
LDADD+=	-lkadm5srv -lkrb5 -ldes -lcrypto -lutil

.include <bsd.prog.mk>
