#	$OpenBSD: Makefile.inc,v 1.4 1998/11/20 11:18:34 d Exp $

.PATH: ${LIBCSRCDIR}/db/btree

SRCS+=	bt_close.c bt_conv.c bt_debug.c bt_delete.c bt_get.c bt_open.c \
	bt_overflow.c bt_page.c bt_put.c bt_search.c bt_seq.c bt_split.c \
	bt_utils.c
