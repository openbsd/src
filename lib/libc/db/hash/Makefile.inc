#	$OpenBSD: Makefile.inc,v 1.4 1998/11/20 11:18:35 d Exp $

.PATH: ${LIBCSRCDIR}/db/hash

SRCS+=	hash.c hash_bigkey.c hash_buf.c hash_func.c hash_log2.c \
	hash_page.c hsearch.c ndbm.c
