#	$OpenBSD: Makefile.inc,v 1.3 1998/05/13 17:15:38 deraadt Exp $

.PATH: ${.CURDIR}/db/man

# mpool.3
MAN+=	btree.3 dbopen.3 hash.3 recno.3 ndbm.3
MLINKS+= dbopen.3 db.3
