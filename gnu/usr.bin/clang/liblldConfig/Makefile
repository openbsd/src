# $OpenBSD: Makefile,v 1.1 2017/01/22 03:13:07 patrick Exp $

LIB=	lldConfig
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/tools/lld/include

.include <bsd.own.mk>
SRCS=	Version.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/lld/lib/Config

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
