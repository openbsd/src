# $OpenBSD: Makefile,v 1.12 2003/05/14 06:59:55 mho Exp $

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
	kadmin_locl.h \
	sl.c

.PATH:	${KRB5SRC}/kadmin
.PATH:	${KRB5SRC}/lib/sl
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
DPADD+= ${LIBREADLINE} ${LIBCURSES} ${LIBHDB} ${LIBKADM5SRV} ${LIBKADM5CLNT} \
	${LIBKRB5} ${LIBASN1} ${LIBCRYPTO} ${LIBCOM_ERR} ${LIBDES}
LDADD+=	-lreadline -lcurses -lhdb -lkadm5srv -lkadm5clnt \
	-lkrb5 ${LIBROKEN} -lasn1 -lcrypto -lcom_err -ldes

.include <bsd.prog.mk>
