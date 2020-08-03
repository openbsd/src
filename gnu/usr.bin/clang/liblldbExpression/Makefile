# $OpenBSD: Makefile,v 1.2 2020/08/03 14:45:29 patrick Exp $

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
	UtilityFunction.cpp \

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Expression

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
