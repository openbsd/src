# $OpenBSD: Makefile,v 1.4 2004/10/29 16:25:30 biorn Exp $

KRB5DIR	= ${.CURDIR}/../../kerberosV
.include <${.CURDIR}/../libkrb5/Makefile.common.inc>

LIB=	kadm5srv

DIR_KADM5SRV=	${KRB5SRC}/lib/kadm5
HDR_KADM5SRV=	admin.h kadm5-private.h kadm5-protos.h private.h
HDR_OBJ=	kadm5_err.h

includes:	kadm5_err.h
	@test -d ${DESTDIR}/usr/include/kerberosV/kadm5 || \
		mkdir ${DESTDIR}/usr/include/kerberosV/kadm5
	@echo -n ${DIR_KADM5SRV}:; \
	cd ${DIR_KADM5SRV}; for i in ${HDR_KADM5SRV}; do \
		echo -n " $$i"; \
		cmp -s $$i ${DESTDIR}/usr/include/kerberosV/kadm5/$$i || \
			${INSTALL} ${INSTALL_COPY} -m 444 \
			-o ${BINOWN} -g $(BINGRP) $$i \
			${DESTDIR}/usr/include/kerberosV/kadm5; done; \
		echo
	@echo -n ${.OBJDIR}:; \
	cd ${.OBJDIR}; for i in ${HDR_OBJ}; do \
		echo -n " $$i"; \
		cmp -s $$i ${DESTDIR}/usr/include/kerberosV/kadm5/$$i || \
			${INSTALL} ${INSTALL_COPY} -m 444 \
			-o ${BINOWN} -g $(BINGRP) $$i \
			${DESTDIR}/usr/include/kerberosV/kadm5; done; \
		echo

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
	password_quality.c 

.PATH:	${KRB5SRC}/lib/kadm5

CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/asn1 -I../asn1 -I../krb5 \
	-I${KRB5SRC}/lib/hdb -I../hdb -I.

CLEANFILES+=kadm5_err.[ch]

kadm5_err.c kadm5_err.h: ${KRB5SRC}/lib/kadm5/kadm5_err.et
	compile_et ${KRB5SRC}/lib/kadm5/kadm5_err.et

.include <bsd.lib.mk>
