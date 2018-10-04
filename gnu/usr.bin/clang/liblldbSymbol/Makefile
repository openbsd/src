# $OpenBSD: Makefile,v 1.1 2018/10/04 11:43:29 patrick Exp $

LIB=	lldbSymbol
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	ArmUnwindInfo.cpp \
	Block.cpp \
	ClangASTContext.cpp \
	ClangASTImporter.cpp \
	ClangExternalASTSourceCallbacks.cpp \
	ClangExternalASTSourceCommon.cpp \
	ClangUtil.cpp \
	CompilerDecl.cpp \
	CompilerDeclContext.cpp \
	CompilerType.cpp \
	CompileUnit.cpp \
	CompactUnwindInfo.cpp \
	DebugMacros.cpp \
	Declaration.cpp \
	DWARFCallFrameInfo.cpp \
	Function.cpp \
	FuncUnwinders.cpp \
	GoASTContext.cpp \
	JavaASTContext.cpp \
	LineEntry.cpp \
	LineTable.cpp \
	ObjectFile.cpp \
	OCamlASTContext.cpp \
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
	VerifyDecl.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Symbol

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
