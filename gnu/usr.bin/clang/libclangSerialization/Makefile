# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:29 patrick Exp $

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
	ModuleFile.cpp \
	ModuleFileExtension.cpp \
	ModuleManager.cpp \
	PCHContainerOperations.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/Serialization

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
