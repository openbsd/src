#	$OpenBSD: Makefile.inc,v 1.4 2015/04/06 09:05:32 tedu Exp $
#
CFLAGS+=-D__DBINTERFACE_PRIVATE

.PATH: ${LIBCSRCDIR}/db/btree

SRCS+=	bt_close.c bt_conv.c bt_debug.c bt_delete.c bt_get.c bt_open.c \
	bt_overflow.c bt_page.c bt_put.c bt_search.c bt_seq.c bt_split.c \
	bt_utils.c

.PATH: ${LIBCSRCDIR}/db/db

SRCS+=	db.c

.PATH: ${LIBCSRCDIR}/db/hash

SRCS+=	hash.c hash_bigkey.c hash_buf.c hash_func.c hash_log2.c \
	hash_page.c ndbm.c

.PATH: ${LIBCSRCDIR}/db/man

MAN+=	btree.3 dbm.3 dbopen.3 hash.3 mpool.3 ndbm.3 recno.3
MLINKS+= dbopen.3 db.3
MLINKS+= dbm.3 dbminit.3 dbm.3 dbmclose.3 dbm.3 fetch.3 dbm.3 store.3
MLINKS+= dbm.3 delete.3 dbm.3 firstkey.3 dbm.3 nextkey.3
MLINKS+= mpool.3 mpool_open.3 mpool.3 mpool_filter.3 mpool.3 mpool_new.3
MLINKS+= mpool.3 mpool_delete.3 mpool.3 mpool_get.3 mpool.3 mpool_put.3
MLINKS+= mpool.3 mpool_sync.3 mpool.3 mpool_close.3
MLINKS+= ndbm.3 dbm_clearerr.3 ndbm.3 dbm_close.3 ndbm.3 dbm_delete.3
MLINKS+= ndbm.3 dbm_dirfno.3 ndbm.3 dbm_error.3 ndbm.3 dbm_fetch.3
MLINKS+= ndbm.3 dbm_firstkey.3 ndbm.3 dbm_nextkey.3 ndbm.3 dbm_open.3
MLINKS+= ndbm.3 dbm_pagfno.3 ndbm.3 dbm_rdonly.3 ndbm.3 dbm_store.3

.PATH: ${LIBCSRCDIR}/db/mpool

SRCS+=	mpool.c

.PATH: ${LIBCSRCDIR}/db/recno

SRCS+=	rec_close.c rec_delete.c rec_get.c rec_open.c rec_put.c rec_search.c \
	rec_seq.c rec_utils.c
