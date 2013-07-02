#$OpenBSD: Makefile,v 1.3 2013/07/02 19:00:09 ajacoutot Exp $

LIB=	heimbase

INCS=	heimbase.h

SRCS= \
	array.c \
	bool.c \
	dict.c \
	heimbase.c \
	null.c \
	number.c \
	string.c

CPPFLAGS=	-I${KRB5DIR}/include

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/base
