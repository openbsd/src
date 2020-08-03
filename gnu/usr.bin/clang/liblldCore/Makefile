# $OpenBSD: Makefile,v 1.6 2020/08/03 14:45:29 patrick Exp $

LIB=	lldCore
NOPROFILE=

CPPFLAGS+=	-I${LLD_SRCS}/include

.include <bsd.own.mk>
SRCS=	DefinedAtom.cpp \
	Error.cpp \
	File.cpp \
	LinkingContext.cpp \
	Reader.cpp \
	Resolver.cpp \
	SymbolTable.cpp \
	Writer.cpp

.PATH:	${.CURDIR}/../../../llvm/lld/lib/Core

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
