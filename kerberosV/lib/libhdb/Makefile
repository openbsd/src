# $OpenBSD: Makefile,v 1.8 2013/11/03 11:56:40 ajacoutot Exp $

LIB=	hdb

MSRC=		${KRB5DIR}/doc/doxyout/hdb/man/man3

INCS=	hdb-protos.h \
	hdb.h

INCS+=	hdb_asn1.h \
	hdb_err.h

SRCS=	common.c \
	db.c \
	db3.c \
	dbinfo.c \
	ext.c \
	hdb-ldap.c \
	hdb.c \
	hdb_err.c \
	hdb_err.h \
	hdb-keytab.c \
	hdb-mitdb.c \
	keys.c \
	keytab.c \
	mkey.c \
	ndbm.c \
	print.c \
	${GEN:S/.x$/.c/:S/.hx$/.h/}

CPPFLAGS=	-I${KRB5DIR}/lib/hdb -I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/krb5 \
		-I.

CFLAGS=	-DHDB_DB_DIR="\"/var/kerberosV\""

GEN=	asn1_Salt.x \
	asn1_Key.x \
	asn1_Event.x \
	asn1_HDBFlags.x \
	asn1_GENERATION.x \
	asn1_HDB_Ext_Constrained_delegation_acl.x \
	asn1_HDB_Ext_Lan_Manager_OWF.x \
	asn1_HDB_Ext_Password.x \
	asn1_HDB_Ext_Aliases.x \
	asn1_HDB_extension.x \
	asn1_HDB_extensions.x \
	asn1_hdb_entry.x \
	asn1_hdb_entry_alias.x \
	asn1_hdb_keyset.x \
	hdb_asn1.hx \
	hdb_asn1-priv.hx

CLEANFILES=	${GEN} ${GEN:S/.x$/.c/:S/.hx$/.h/} hdb_asn1_files \
		hdb_asn1-template.[ch]*

${GEN}: hdb.asn1
	${ASN1_COMPILE} ${.ALLSRC:M*.asn1} hdb_asn1

.SUFFIXES: .h .c .x .hx

.x.c:
	cmp -s ${.IMPSRC} ${.TARGET} 2> /dev/null || cp ${.IMPSRC} ${.TARGET}

.hx.h:
	cmp -s ${.IMPSRC} ${.TARGET} 2> /dev/null || cp ${.IMPSRC} ${.TARGET}

.include <bsd.lib.mk>

.SUFFIXES: .h .c .x .hx

.PATH: ${KRB5DIR}/lib/hdb ${KRB5DIR}/doc/doxyout/hdb/man/man3/
