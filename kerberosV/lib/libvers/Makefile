# $OpenBSD: Makefile,v 1.2 2013/07/02 19:00:10 ajacoutot Exp $

LIB=	vers
SRCS=	print_version.c roken.h
CPPFLAGS=	-I. -I${KRB5DIR}/lib/roken

# skip installation
install:

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/vers ${KRB5DIR}/include ${.CURDIR}/../../include
