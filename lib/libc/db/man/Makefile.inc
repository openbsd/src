#	$OpenBSD: Makefile.inc,v 1.7 1998/11/20 11:18:35 d Exp $

.PATH: ${LIBCSRCDIR}/db/man

MAN+=	btree.3 dbopen.3 hash.3 recno.3 ndbm.3 mpool.3
MLINKS+= dbopen.3 db.3
MLINKS+= ndbm.3 dbm.3
