# $OpenBSD: Makefile,v 1.6 2023/11/11 18:35:39 robert Exp $

LIB=	lldbPluginExpressionParser
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

SRCS=	ASTResultSynthesizer.cpp \
	ASTStructExtractor.cpp \
	ASTUtils.cpp \
	ClangASTImporter.cpp \
	ClangASTMetadata.cpp \
	ClangASTSource.cpp \
	ClangDeclVendor.cpp \
	ClangExpressionDeclMap.cpp \
	ClangExpressionParser.cpp \
	ClangExpressionSourceCode.cpp \
	ClangExpressionUtil.cpp \
	ClangExpressionVariable.cpp \
	ClangExternalASTSourceCallbacks.cpp \
	ClangFunctionCaller.cpp \
	ClangHost.cpp \
	ClangModulesDeclVendor.cpp \
	ClangPersistentVariables.cpp \
	ClangUserExpression.cpp \
	ClangUtil.cpp \
	ClangUtilityFunction.cpp \
	CppModuleConfiguration.cpp \
	CxxModuleHandler.cpp \
	IRForTarget.cpp \
	IRDynamicChecks.cpp \
	NameSearchContext.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ExpressionParser/Clang

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
