#	$OpenBSD: Makefile.inc,v 1.3 1996/08/19 08:20:54 tholo Exp $

.PATH: ${.CURDIR}/db/recno

SRCS+=	rec_close.c rec_delete.c rec_get.c rec_open.c rec_put.c rec_search.c \
	rec_seq.c rec_utils.c
