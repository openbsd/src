#$OpenBSD: Makefile,v 1.2 2013/06/21 10:59:40 ajacoutot Exp $

LIB=	wind

INCS=	wind.h \
	wind_err.h

SRCS=	bidi.c \
	bidi_table.c \
	combining.c \
	combining_table.c \
	doxygen.c \
	errorlist.c \
	errorlist_table.c \
	ldap.c \
	map.c \
	map_table.c \
	normalize.c \
	normalize_table.c \
	punycode.c \
	stringprep.c \
	utf8.c

SRCS+=	wind_err.c \
	wind_err.h

CFLAGS+=-I${KRB5DIR}/lib/roken -I.

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/wind
