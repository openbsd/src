# $OpenBSD: Makefile,v 1.5 2026/05/29 11:18:31 robert Exp $

LIB=	clangSerialization
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>
SRCS=	ASTCommon.cpp \
	ASTReader.cpp \
	ASTReaderDecl.cpp \
	ASTReaderStmt.cpp \
	ASTWriter.cpp \
	ASTWriterDecl.cpp \
	ASTWriterStmt.cpp \
	GeneratePCH.cpp \
	GlobalModuleIndex.cpp \
	InMemoryModuleCache.cpp \
	ModuleCache.cpp \
	ModuleFile.cpp \
	ModuleFileExtension.cpp \
	ModuleManager.cpp \
	PCHContainerOperations.cpp \
	ObjectFilePCHContainerReader.cpp \
	TemplateArgumentHasher.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/Serialization

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
