# $OpenBSD: Makefile,v 1.9 2006/04/23 20:46:55 biorn Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	ipropd-slave
NOMAN=

SRCS=	ipropd_slave.c
SRCS+=	${PRINTVER}

.PATH:	${KRB5SRC}/lib/kadm5

CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5

DPADD+= ${LIBKADM5SRV} ${LIBKRB5} ${LIBCRYPTO} ${LIBUTIL} ${LIBDES}
LDADD+=	-lkadm5srv -lkrb5 -lcrypto -lutil -ldes

.include <bsd.prog.mk>
