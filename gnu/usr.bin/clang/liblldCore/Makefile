# $OpenBSD: Makefile,v 1.2 2017/01/24 08:44:47 patrick Exp $

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

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
