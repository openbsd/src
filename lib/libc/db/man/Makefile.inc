#	$OpenBSD: Makefile.inc,v 1.9 1999/04/18 17:08:10 millert Exp $

.PATH: ${LIBCSRCDIR}/db/man

MAN+=	btree.3 dbm.3 dbopen.3 hash.3 mpool.3 ndbm.3 recno.3
MLINKS+= dbopen.3 db.3
MLINKS+= dbm.3 dbminit.3 dbm.3 dbmclose.3 dbm.3 fetch.3 dbm.3 store.3
MLINKS+= dbm.3 delete.3 dbm.3 firstkey.3 dbm.3 nextkey.3
MLINKS+= ndbm.3 dbm_clearerr.3 ndbm.3 dbm_close.3 ndbm.3 dbm_delete.3
MLINKS+= ndbm.3 dbm_dirfno.3 ndbm.3 dbm_error.3 ndbm.3 dbm_fetch.3
MLINKS+= ndbm.3 dbm_firstkey.3 ndbm.3 dbm_nextkey.3 ndbm.3 dbm_open.3
MLINKS+= ndbm.3 dbm_pagfno.3 ndbm.3 dbm_store.3
