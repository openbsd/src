#	$OpenBSD: Makefile.inc,v 1.5 1998/05/14 02:12:47 millert Exp $

.PATH: ${.CURDIR}/db/man

# mpool.3
MAN+=	btree.3 dbopen.3 hash.3 recno.3 ndbm.3
MLINKS+= dbopen.3 db.3
MLINKS+= ndbm.3 dbm.3
