# $OpenBSD: Makefile,v 1.6 2023/11/11 18:35:40 robert Exp $

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
	ObjectContainer.cpp \
	ObjectFile.cpp \
	PostfixExpression.cpp \
	Symbol.cpp \
	SymbolContext.cpp \
	SymbolFile.cpp \
	SymbolFileOnDemand.cpp \
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
