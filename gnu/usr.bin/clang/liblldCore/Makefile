# $OpenBSD: Makefile,v 1.5 2018/04/06 14:44:04 patrick Exp $

LIB=	lldCore
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/tools/lld/include

.include <bsd.own.mk>
SRCS=	DefinedAtom.cpp \
	Error.cpp \
	File.cpp \
	LinkingContext.cpp \
	Reader.cpp \
	Resolver.cpp \
	SymbolTable.cpp \
	Writer.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/lld/lib/Core

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
