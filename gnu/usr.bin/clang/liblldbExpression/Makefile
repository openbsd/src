# $OpenBSD: Makefile,v 1.1 2018/10/04 11:43:29 patrick Exp $

LIB=	lldbExpression
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

SRCS=	DiagnosticManager.cpp \
	DWARFExpression.cpp \
	Expression.cpp \
	ExpressionSourceCode.cpp \
	ExpressionVariable.cpp \
	FunctionCaller.cpp \
	IRDynamicChecks.cpp \
	IRExecutionUnit.cpp \
	IRInterpreter.cpp \
	IRMemoryMap.cpp \
	LLVMUserExpression.cpp \
	Materializer.cpp \
	REPL.cpp \
	UserExpression.cpp \
	UtilityFunction.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Expression

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
