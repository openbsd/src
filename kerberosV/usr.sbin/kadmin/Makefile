# $OpenBSD: Makefile,v 1.6 2001/09/12 05:09:52 jason Exp $

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

.if (${ELF_TOOLCHAIN:L} == "yes" && ${MACHINE_ARCH} != "sparc64")
LDADD+= -ldl
.endif
