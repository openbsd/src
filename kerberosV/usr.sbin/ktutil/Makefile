# $OpenBSD: Makefile,v 1.8 2003/04/07 21:31:45 hin Exp $

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
	rename.c

.PATH:	${KRB5SRC}/admin
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
DPADD+=	${LIBREADLINE} ${LIBCURSES} ${LIBKRB} ${LIBHDB} ${LIBADM5SRV} \
	${LIBKADM5CLNT} ${LIBKRB5} ${LIBASN1} ${LIBCRYPTO} ${LIBCOM_ERR} \
	${LIBKAFS} ${LIBDES}
LDADD+=	${LIBSL} -lreadline -lcurses -lkrb -lhdb -lkadm5clnt \
	-lhdb -lkrb5 ${LIBROKEN} -lasn1 -lcrypto -lcom_err -lkafs -ldes

.include <bsd.prog.mk>
