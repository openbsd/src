# $OpenBSD: Makefile,v 1.5 2021/04/28 12:55:41 patrick Exp $

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
