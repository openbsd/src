#	$OpenBSD: Makefile.inc,v 1.5 2004/06/24 04:43:33 millert Exp $

.PATH: ${LIBCSRCDIR}/db/hash

SRCS+=	hash.c hash_bigkey.c hash_buf.c hash_func.c hash_log2.c \
	hash_page.c ndbm.c
