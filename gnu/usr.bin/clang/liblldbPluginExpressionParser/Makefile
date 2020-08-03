# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:30 patrick Exp $

LIB=	lldbPluginExpressionParser
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	ASTResultSynthesizer.cpp \
	ASTStructExtractor.cpp \
	ASTUtils.cpp \
	ClangASTSource.cpp \
	ClangDeclVendor.cpp \
	ClangExpressionDeclMap.cpp \
	ClangExpressionParser.cpp \
	ClangExpressionSourceCode.cpp \
	ClangExpressionVariable.cpp \
	ClangFunctionCaller.cpp \
	ClangHost.cpp \
	ClangModulesDeclVendor.cpp \
	ClangPersistentVariables.cpp \
	ClangUserExpression.cpp \
	ClangUtilityFunction.cpp \
	CppModuleConfiguration.cpp \
	IRForTarget.cpp \
	IRDynamicChecks.cpp \

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/ExpressionParser/Clang

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
