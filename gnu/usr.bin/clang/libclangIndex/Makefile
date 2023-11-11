# $OpenBSD: Makefile,v 1.1 2023/11/11 18:35:38 robert Exp $

LIB=	clangIndex
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>

SRCS=	CommentToXML.cpp \
	FileIndexRecord.cpp \
	IndexBody.cpp \
	IndexDecl.cpp \
	IndexingAction.cpp \
	IndexingContext.cpp \
	IndexSymbol.cpp \
	IndexTypeSourceInfo.cpp \
	USRGeneration.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/Index

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
