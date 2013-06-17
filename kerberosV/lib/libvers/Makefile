# $OpenBSD: Makefile,v 1.1 2013/06/17 19:11:35 robert Exp $

LIB=	vers
SRCS=	print_version.c roken.h
CFLAGS+=-I. -I${KRB5DIR}/lib/roken

# skip installation
install:

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/vers ${KRB5DIR}/include ${.CURDIR}/../../include
