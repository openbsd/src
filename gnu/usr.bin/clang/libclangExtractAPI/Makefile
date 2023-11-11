# $OpenBSD: Makefile,v 1.1 2023/11/11 18:35:38 robert Exp $

LIB=	clangExtractAPI
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>

SRCS=	API.cpp \
	APIIgnoresList.cpp \
	AvailabilityInfo.cpp \
	ExtractAPIConsumer.cpp \
	ExtractAPIVisitor.cpp \
	DeclarationFragments.cpp \
	TypedefUnderlyingTypeResolver.cpp

# Serialization
SRCS+=	SerializerBase.cpp \
	SymbolGraphSerializer.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/ExtractAPI
.PATH:	${.CURDIR}/../../../llvm/clang/lib/ExtractAPI/Serialization

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
