# $OpenBSD: Makefile.hdb.inc,v 1.2 2003/05/17 05:20:18 mho Exp $

DIR_HDB=	${KRB5SRC}/lib/hdb
HDR_HDB=	hdb.h hdb-protos.h hdb-private.h
HDR_OBJ+=	hdb_err.h hdb_asn1.h

includes:	hdb_err.h hdb_asn1.h

ASN1_SRC = asn1_Key.c asn1_Event.c asn1_HDBFlags.c asn1_hdb_entry.c \
        asn1_Salt.c hdb_asn1.h asn1_GENERATION.c

SRCS+=	${ASN1_SRC} \
	keytab.c \
	hdb.c \
	common.c \
	db.c \
	mkey.c \
	ndbm.c \
	print.c \
	hdb_err.c \
	hdb_err.h

${ASN1_SRC}:
	asn1_compile ${KRB5SRC}/lib/hdb/hdb.asn1 hdb_asn1
	for f in *.x; do mv $$f `echo $$f | sed s,.x$$,.c,g`; done

CLEANFILES+=	hdb_err.[ch] ${ASN1_SRC} \
		hdb_asn1.[ch] ans1_files asn1.h asn1_files

CFLAGS+=-I${KRB5SRC}/lib/hdb -I${KRB5SRC}/lib/asn1

.PATH:	${KRB5SRC}/lib/hdb

hdb_err.c hdb_err.h: ${KRB5SRC}/lib/hdb/hdb_err.et
	compile_et ${KRB5SRC}/lib/hdb/hdb_err.et

