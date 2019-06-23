# $OpenBSD: Makefile,v 1.3 2019/06/23 22:07:42 patrick Exp $

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
	Module.cpp \
	ModuleFileExtension.cpp \
	ModuleManager.cpp \
	PCHContainerOperations.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/clang/lib/Serialization

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
