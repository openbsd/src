#	$OpenBSD: Makefile.inc,v 1.4 1998/11/20 11:18:36 d Exp $

.PATH: ${LIBCSRCDIR}/db/recno

SRCS+=	rec_close.c rec_delete.c rec_get.c rec_open.c rec_put.c rec_search.c \
	rec_seq.c rec_utils.c
