#	$OpenBSD: Makefile.inc,v 1.1 2002/02/22 21:06:00 drahn Exp $

.PATH: ${.CURDIR}/../libc/posix1e

SRCS+=	extattr_name.c

MAN+=	posix1e.3			\
	extattr.3

MLINKS+= extattr.3 extattr_namespace_to_string.3 \
        extattr.3 extattr_string_to_namespace.3
