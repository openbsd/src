# $OpenBSD: Makefile,v 1.7 2001/09/27 05:04:13 art Exp $

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
	version4.c \
	util.c \
	random_password.c \
	kadmin_locl.h \
	kadm_conn.c

.PATH:	${KRB5SRC}/kadmin
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
LDADD+=	${LIBSL} -lreadline -lcurses -lhdb -lkadm5srv \
	-lkadm5clnt -lkrb5 ${LIBROKEN} -lkrb -lasn1 -lcrypto -lcom_err -lkafs

.include <bsd.prog.mk>

.if (${ELF_TOOLCHAIN:L} == "yes")
LDADD+= -ldl
.endif
