# $OpenBSD: Makefile,v 1.3 2021/04/28 12:55:41 patrick Exp $

LIB=	lldbExpression
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

SRCS=	DiagnosticManager.cpp \
	DWARFExpression.cpp \
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
