#$OpenBSD: Makefile,v 1.3 2013/07/02 19:00:10 ajacoutot Exp $

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

CPPFLAGS=	-I${KRB5DIR}/lib/roken -I.

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/wind
