#	$OpenBSD: Makefile.inc,v 1.2 1996/08/19 08:20:46 tholo Exp $

.PATH: ${.CURDIR}/db/man

# mpool.3
MAN+=	btree.3 dbopen.3 hash.3 recno.3
MLINKS+= dbopen.3 db.3
