# $OpenBSD: Makefile,v 1.1 2003/05/14 23:48:20 hin Exp $

KRB5DIR	= ${.CURDIR}/../../kerberosV
.include <${.CURDIR}/../libkrb5/Makefile.common.inc>

LIB=	gssapi

INCLUDE_FILES=	${KRB5SRC}/lib/gssapi/gssapi.h

beforedepend:	krb5_err.h heim_err.h

CLEANFILES+=	krb5_err.[ch] heim_err.[ch]

SRCS=	8003.c			\
	accept_sec_context.c	\
	acquire_cred.c		\
	add_oid_set_member.c	\
	canonicalize_name.c	\
	compare_name.c		\
	compat.c		\
	context_time.c		\
	create_emtpy_oid_set.c	\
	decapsulate.c		\
	delete_sec_context.c	\
	display_name.c		\
	display_status.c	\
	duplicate_name.c	\
	encapsulate.c		\
	export_sec_context.c	\
	export_name.c		\
	external.c		\
	get_mic.c		\
	gssapi.h		\
	gssapi_locl.h		\
	import_name.c		\
	import_sec_context.c	\
	indicate_mechs.c	\
	init.c			\
	init_sec_context.c	\
	inquire_context.c	\
	inquire_cred.c		\
	release_buffer.c	\
	release_cred.c		\
	release_name.c		\
	release_oid_set.c	\
	test_oid_set_member.c	\
	unwrap.c		\
	v1.c			\
	verify_mic.c		\
        wrap.c                  \
        address_to_krb5addr.c


.PATH:	${KRB5SRC}/lib/gssapi
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/ \
	-I${KRB5SRC}/lib/asn1 -I../asn1 -I. \
	-DSHA1_CTX=SHA_CTX

.include <bsd.lib.mk>