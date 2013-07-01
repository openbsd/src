# $OpenBSD: Makefile,v 1.25 2013/07/01 05:15:24 ajacoutot Exp $

PROG=	kadmin
MPAGES=	kadmin.8
MSRC=	${KRB5DIR}/kadmin

SRCS=	add_enctype.c \
	ank.c \
	check.c \
	cpw.c \
	del.c \
	del_enctype.c \
	dump.c \
	ext.c \
	get.c \
	init.c \
	kadmin.c \
	kadmin-commands.c \
	load.c \
	mod.c \
	pw_quality.c \
	random_password.c \
	rename.c \
	stash.c \
	util.c

CFLAGS+=-I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/roken \
	-I${KRB5DIR}/lib/sl -I. ${LDAPCFLAGS}
DPADD=	${LIBKADM5CLNT} ${LIBKADM5SRV} ${LIBHDB} ${LIBKRB5} ${LIBHX509} \
	${LIBSL} ${LIBROKEN} ${LIBVERS} ${LIBASN1} \
	${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} \
	${LIBEDIT} ${LIBNCURSES} ${LDAPDPADD}
LDADD=	-lkadm5clnt -lkadm5srv -lhdb -lkrb5 \
	${LIBSL} -lroken ${LIBVERS} -lasn1 \
	-lcrypto -lcom_err \
	-ledit -lncurses -lwind -lheimbase

CLEANFILES+=	kadmin-commands.[ch]*

.include <bsd.prog.mk>

kadmin-commands.h kadmin-commands.c: ${KRB5DIR}/kadmin/kadmin-commands.in
	${SLC} ${.ALLSRC:M*.in}

kadmin-commands.o: kadmin-commands.h

.PATH: ${KRB5DIR}/kadmin
