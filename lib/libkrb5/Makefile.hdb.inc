# $OpenBSD: Makefile.hdb.inc,v 1.4 2003/05/17 21:17:39 mho Exp $

DIR_HDB=	${KRB5SRC}/lib/hdb
HDR_HDB=	hdb.h hdb-protos.h hdb-private.h
HDR_GEN+=	hdb_err.h hdb_asn1.h

.PATH:	${KRB5SRC}/lib/hdb

gen_files_hdb= \
	hdb_asn1.h \
	asn1_Key.c \
	asn1_Event.c \
	asn1_HDBFlags.c \
	asn1_hdb_entry.c \
        asn1_Salt.c \
	asn1_GENERATION.c

gen_files_hdb+=\
	hdb_err.h \
	hdb_err.c

SRCS+=	keytab.c \
	hdb.c \
	common.c \
	db.c \
	mkey.c \
	ndbm.c \
	print.c \
	${gen_files_hdb}

CFLAGS+=	-I${KRB5SRC}/lib/hdb -I${KRB5SRC}/lib/asn1

regen_hdb:
	cd ${GENDIR}; compile_et ${KRB5SRC}/lib/hdb/hdb_err.et
	cd ${GENDIR}; asn1_compile ${KRB5SRC}/lib/hdb/hdb.asn1 hdb_asn1; \
		for f in *.x; do mv $$f `echo $$f | sed s,.x$$,.c,g`; done

regen:		regen_hdb
