# $OpenBSD: Makefile.hdb.inc,v 1.6 2006/04/14 08:24:41 biorn Exp $

DIR_HDB=	${KRB5SRC}/lib/hdb
HDR_HDB=	hdb.h hdb-protos.h hdb-private.h
HDR_GEN+=	hdb_err.h hdb_asn1.h

.PATH:	${DIR_HDB}

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
	keys.c \
	hdb.c \
	common.c \
	db.c \
	mkey.c \
	ndbm.c \
	print.c \
	${gen_files_hdb}

CFLAGS+=	-I${DIR_HDB} #-I${DIR_ASN1}

regen_hdb:
	cd ${GENDIR}; compile_et ${DIR_HDB}/hdb_err.et
	cd ${GENDIR}; asn1_compile ${DIR_HDB}/hdb.asn1 hdb_asn1; \
		for f in *.x; do mv $$f `echo $$f | sed s,.x$$,.c,g`; done

regen:		regen_hdb
