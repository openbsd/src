# $OpenBSD: Makefile,v 1.2 2005/11/24 20:49:18 deraadt Exp $

KRB5DIR	= ${.CURDIR}/../../kerberosV
.include <${.CURDIR}/../libkrb5/Makefile.common.inc>

LIB=	kadm5clnt
WANTLINT=

INCLUDE_FILES=	${KRB5SRC}/lib/kadm5/{admin.h,private.h} kadm5_err.h

includes:	kadm5_err.h

SRCS=	admin.h         \
	chpass_c.c      \
	common_glue.c   \
	client_glue.c	\
	create_c.c      \
	delete_c.c      \
	destroy_c.c     \
	flush_c.c       \
	free.c          \
	get_c.c         \
	get_princs_c.c  \
	init_c.c        \
	kadm5_err.c     \
	kadm5_locl.h    \
	marshall.c      \
	modify_c.c      \
	private.h       \
	privs_c.c       \
	randkey_c.c     \
	rename_c.c      \
	send_recv.c     \
	kadm5_err.c	\
	kadm5_err.h

.PATH:	${KRB5SRC}/lib/kadm5

CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/asn1 -I../asn1 -I../krb5 \
	-I${KRB5SRC}/lib/hdb -I../hdb -I.

CLEANFILES+=kadm5_err.[ch]

kadm5_err.c kadm5_err.h: ${KRB5SRC}/lib/kadm5/kadm5_err.et
	compile_et ${KRB5SRC}/lib/kadm5/kadm5_err.et

.include <bsd.lib.mk>
