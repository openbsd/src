# $OpenBSD: Makefile,v 1.1 2003/05/14 23:48:20 hin Exp $

KRB5DIR	= ${.CURDIR}/../../kerberosV
.include <${.CURDIR}/../libkrb5/Makefile.common.inc>

LIB=	kadm5srv

INCLUDE_DIR=	${DESTDIR}/usr/include/kerberosV/kadm5
INCLUDE_FILES=	${KRB5SRC}/lib/kadm5/{admin.h,private.h} kadm5_err.h

includes:	kadm5_err.h

SRCS=	acl.c           \
	admin.h         \
	bump_pw_expire.c \
	chpass_s.c      \
	common_glue.c   \
	context_s.c     \
	create_s.c      \
	delete_s.c      \
	destroy_s.c     \
	ent_setup.c     \
	error.c         \
	flush_s.c       \
	free.c          \
	get_princs_s.c  \
	get_s.c         \
	init_s.c        \
	kadm5_err.c     \
	kadm5_locl.h    \
	keys.c		\
	log.c           \
	marshall.c      \
	modify_s.c      \
	private.h       \
	privs_s.c       \
	randkey_s.c     \
	rename_s.c      \
	server_glue.c	\
	set_keys.c      \
	set_modifier.c  \
	password_quality.c \
	kadm5_err.c	\
	kadm5_err.h

.PATH:	${KRB5SRC}/lib/kadm5

CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/asn1 -I../asn1 -I../krb5 \
	-I${KRB5SRC}/lib/hdb -I../hdb -I.

CLEANFILES+=kadm5_err.[ch]

kadm5_err.c kadm5_err.h: ${KRB5SRC}/lib/kadm5/kadm5_err.et
	compile_et ${KRB5SRC}/lib/kadm5/kadm5_err.et

.include <bsd.lib.mk>
