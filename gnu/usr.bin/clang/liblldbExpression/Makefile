# $OpenBSD: Makefile,v 1.4 2023/11/11 18:35:39 robert Exp $

LIB=	lldbExpression
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

SRCS=	DiagnosticManager.cpp \
	DWARFExpression.cpp \
	DWARFExpressionList.cpp \
	Expression.cpp \
	ExpressionVariable.cpp \
	FunctionCaller.cpp \
	IRExecutionUnit.cpp \
	IRInterpreter.cpp \
	IRMemoryMap.cpp \
	LLVMUserExpression.cpp \
	Materializer.cpp \
	REPL.cpp \
	UserExpression.cpp \
	UtilityFunction.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Expression

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
