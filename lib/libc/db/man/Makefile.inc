#	$NetBSD: Makefile.inc,v 1.5 1995/02/27 13:23:12 cgd Exp $
#       @(#)Makefile.inc	8.1 (Berkeley) 6/4/93

.PATH: ${.CURDIR}/db/man

# mpool.3
MAN+=	btree.3 dbopen.3 hash.3 recno.3
MLINKS+= dbopen.3 db.3
