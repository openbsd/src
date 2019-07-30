# $OpenBSD: Makefile,v 1.4 2017/10/04 20:57:10 patrick Exp $

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
	TargetOptionsCommandFlags.cpp \
	Writer.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/lld/lib/Core

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
