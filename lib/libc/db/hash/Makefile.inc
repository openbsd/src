#	$OpenBSD: Makefile.inc,v 1.3 1996/08/19 08:20:27 tholo Exp $

.PATH: ${.CURDIR}/db/hash

SRCS+=	hash.c hash_bigkey.c hash_buf.c hash_func.c hash_log2.c \
	hash_page.c hsearch.c ndbm.c
