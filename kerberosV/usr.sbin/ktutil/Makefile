# $OpenBSD: Makefile,v 1.9 2003/05/14 07:18:27 mho Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	ktutil
MAN=	ktutil.8

SRCS=	add.c \
	change.c \
	copy.c \
	get.c \
	ktutil.c \
	list.c \
	purge.c \
	remove.c \
	rename.c \
	sl.c

.PATH:	${KRB5SRC}/admin
.PATH:	${KRB5SRC}/lib/sl
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
DPADD+=	${LIBREADLINE} ${LIBCURSES} ${LIBKADM5CLNT} ${LIBHDB} \
	${LIBKRB5} ${LIBASN1} ${LIBCRYPTO} ${LIBCOM_ERR} ${LIBDES}
LDADD+=	-lreadline -lcurses -lkadm5clnt -lhdb \
	-lkrb5 ${LIBROKEN} -lasn1 -lcrypto -lcom_err -ldes

.include <bsd.prog.mk>
