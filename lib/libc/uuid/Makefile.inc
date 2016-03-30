#	$OpenBSD: Makefile.inc,v 1.2 2016/03/30 06:38:42 jmc Exp $
#	$NetBSD: Makefile.inc,v 1.1 2004/09/13 21:44:54 thorpej Exp $

# DCE 1.1 compatible UUID implementation sources

.PATH: ${LIBCSRCDIR}/uuid

SRCS+=	uuid_compare.c uuid_create.c uuid_create_nil.c uuid_equal.c \
	uuid_from_string.c uuid_hash.c uuid_is_nil.c uuid_stream.c \
	uuid_to_string.c

MAN+=	uuid.3
