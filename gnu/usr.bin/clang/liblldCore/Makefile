# $OpenBSD: Makefile,v 1.3 2017/07/09 15:28:36 espie Exp $

LIB=	lldCore
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/tools/lld/include

.include <bsd.own.mk>
SRCS=	DefinedAtom.cpp \
	Error.cpp \
	File.cpp \
	LinkingContext.cpp \
	Reader.cpp \
	Reproduce.cpp \
	Resolver.cpp \
	SymbolTable.cpp \
	Writer.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/lld/lib/Core

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
