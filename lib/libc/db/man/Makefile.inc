#	$OpenBSD: Makefile.inc,v 1.6 1998/07/23 07:18:01 deraadt Exp $

.PATH: ${.CURDIR}/db/man

MAN+=	btree.3 dbopen.3 hash.3 recno.3 ndbm.3 mpool.3
MLINKS+= dbopen.3 db.3
MLINKS+= ndbm.3 dbm.3
