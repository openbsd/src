# $OpenBSD: Makefile,v 1.11 2003/05/11 04:10:55 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kadmin
MAN=	kadmin.8

SRCS=	ank.c \
	cpw.c \
	del.c \
	del_enctype.c \
	dump.c \
	ext.c \
	get.c \
	init.c \
	kadmin.c \
	load.c \
	mod.c \
	rename.c \
	util.c \
	random_password.c \
	kadmin_locl.h

.PATH:	${KRB5SRC}/kadmin
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
DPADD+= ${LIBREADLINE} ${LIBCURSES} ${LIBHDB} ${LIBKADM5SRV} ${LIBKADM5CLNT} \
	${LIBKRB5} ${LIBKRB} ${LIBASN1} ${LIBCRYPTO} ${LIBCOM_ERR} ${LIBKAFS} \
	${LIBDES}
LDADD+=	${LIBSL} -lreadline -lcurses -lhdb -lkadm5srv \
	-lkadm5clnt -lkrb5 ${LIBROKEN} -lkrb -lasn1 -lcrypto -lcom_err -lkafs -ldes

.include <bsd.prog.mk>
