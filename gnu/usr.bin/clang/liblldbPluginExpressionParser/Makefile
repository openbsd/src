# $OpenBSD: Makefile,v 1.1 2018/10/04 11:43:29 patrick Exp $

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
	ClangModulesDeclVendor.cpp \
	ClangPersistentVariables.cpp \
	ClangUserExpression.cpp \
	ClangUtilityFunction.cpp \
	GoLexer.cpp \
	GoParser.cpp \
	GoUserExpression.cpp \
	IRForTarget.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/ExpressionParser/Clang
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/ExpressionParser/Go

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
