# $OpenBSD: Makefile,v 1.3 2019/06/23 22:07:43 patrick Exp $

LIB=	lldbPluginExpressionParser
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	ASTDumper.cpp \
	ASTResultSynthesizer.cpp \
	ASTStructExtractor.cpp \
	ClangASTSource.cpp \
	ClangExpressionDeclMap.cpp \
	ClangExpressionParser.cpp \
	ClangExpressionVariable.cpp \
	ClangFunctionCaller.cpp \
	ClangHost.cpp \
	ClangModulesDeclVendor.cpp \
	ClangPersistentVariables.cpp \
	ClangUserExpression.cpp \
	ClangUtilityFunction.cpp \
	IRForTarget.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/ExpressionParser/Clang

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
