# $OpenBSD: Makefile,v 1.5 2021/12/17 14:55:47 patrick Exp $

LIB=	lldbSymbol
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	ArmUnwindInfo.cpp \
	Block.cpp \
	CompactUnwindInfo.cpp \
	CompileUnit.cpp \
	CompilerDecl.cpp \
	CompilerDeclContext.cpp \
	CompilerType.cpp \
	DWARFCallFrameInfo.cpp \
	DebugMacros.cpp \
	DeclVendor.cpp \
	FuncUnwinders.cpp \
	Function.cpp \
	LineEntry.cpp \
	LineTable.cpp \
	LocateSymbolFile.cpp \
	ObjectFile.cpp \
	PostfixExpression.cpp \
	Symbol.cpp \
	SymbolContext.cpp \
	SymbolFile.cpp \
	SymbolVendor.cpp \
	Symtab.cpp \
	Type.cpp \
	TypeList.cpp \
	TypeMap.cpp \
	TypeSystem.cpp \
	UnwindPlan.cpp \
	UnwindTable.cpp \
	Variable.cpp \
	VariableList.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Symbol

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
