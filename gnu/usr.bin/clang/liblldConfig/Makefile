# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:36 espie Exp $

LIB=	lldConfig
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/tools/lld/include

.include <bsd.own.mk>
SRCS=	Version.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/lld/lib/Config

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
