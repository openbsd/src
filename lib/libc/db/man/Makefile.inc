#	$OpenBSD: Makefile.inc,v 1.8 1999/02/15 05:11:24 millert Exp $

.PATH: ${LIBCSRCDIR}/db/man

MAN+=	btree.3 dbm.3 dbopen.3 hash.3 mpool.3 ndbm.3 recno.3
MLINKS+= dbopen.3 db.3
