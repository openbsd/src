# $OpenBSD: Makefile,v 1.5 2025/06/11 17:46:33 robert Exp $

LIB=	lldbExpression
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

SRCS=	DiagnosticManager.cpp \
	DWARFExpression.cpp \
	DWARFExpressionList.cpp \
	Expression.cpp \
	ExpressionParser.cpp \
	ExpressionTypeSystemHelper.cpp \
	ExpressionVariable.cpp \
	FunctionCaller.cpp \
	IRExecutionUnit.cpp \
	IRInterpreter.cpp \
	IRMemoryMap.cpp \
	LLVMUserExpression.cpp \
	Materializer.cpp \
	ObjectFileJIT.cpp \
	REPL.cpp \
	UserExpression.cpp \
	UtilityFunction.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Expression

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
