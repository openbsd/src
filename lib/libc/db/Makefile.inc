#	$OpenBSD: Makefile.inc,v 1.3 1998/11/20 11:18:33 d Exp $
#
CFLAGS+=-D__DBINTERFACE_PRIVATE

.include "${LIBCSRCDIR}/db/btree/Makefile.inc"
.include "${LIBCSRCDIR}/db/db/Makefile.inc"
.include "${LIBCSRCDIR}/db/hash/Makefile.inc"
.include "${LIBCSRCDIR}/db/man/Makefile.inc"
.include "${LIBCSRCDIR}/db/mpool/Makefile.inc"
.include "${LIBCSRCDIR}/db/recno/Makefile.inc"
