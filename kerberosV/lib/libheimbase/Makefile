#$OpenBSD: Makefile,v 1.2 2013/06/21 10:59:40 ajacoutot Exp $

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

CFLAGS+=	-I${KRB5DIR}/include

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/base
