# $OpenBSD: Makefile,v 1.14 2004/06/04 23:45:19 biorn Exp $

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
SRCS+=	${PRINTVER}
SRCS+=	tm2time.c

.PATH:	${KRB5SRC}/kadmin
.PATH:	${KRB5SRC}/lib/sl
.PATH:	${ROKENSRC}

CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5

DPADD+= ${LIBREADLINE} ${LIBCURSES} ${LIBKADM5SRV} ${LIBKADM5CLNT} \
	${LIBKRB5} ${LIBCRYPTO} ${LIBDES}
LDADD+=	-lreadline -lcurses -lkadm5srv -lkadm5clnt \
	-lkrb5 -lcrypto -ldes

.include <bsd.prog.mk>
