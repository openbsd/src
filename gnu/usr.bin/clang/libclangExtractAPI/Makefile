# $OpenBSD: Makefile,v 1.2 2025/06/11 17:46:33 robert Exp $

LIB=	clangExtractAPI
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>

SRCS=	API.cpp \
	APIIgnoresList.cpp \
	ExtractAPIConsumer.cpp \
	DeclarationFragments.cpp \
	TypedefUnderlyingTypeResolver.cpp

# Serialization
SRCS+=	SymbolGraphSerializer.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/ExtractAPI
.PATH:	${.CURDIR}/../../../llvm/clang/lib/ExtractAPI/Serialization

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
