# $OpenBSD: Makefile,v 1.3 2020/08/03 14:45:31 patrick Exp $

LIB=	lldbSymbol
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	ArmUnwindInfo.cpp \
	Block.cpp \
	ClangASTContext.cpp \
	ClangASTImporter.cpp \
	ClangASTMetadata.cpp \
	ClangExternalASTSourceCallbacks.cpp \
	ClangUtil.cpp \
	CompactUnwindInfo.cpp \
	CompileUnit.cpp \
	CompilerDecl.cpp \
	CompilerDeclContext.cpp \
	CompilerType.cpp \
	CxxModuleHandler.cpp \
	DWARFCallFrameInfo.cpp \
	DebugMacros.cpp \
	Declaration.cpp \
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
	VariableList.cpp \

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Symbol

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
