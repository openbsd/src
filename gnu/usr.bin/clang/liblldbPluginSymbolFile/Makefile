# $OpenBSD: Makefile,v 1.6 2023/11/11 18:35:39 robert Exp $

LIB=	lldbPluginSymbolFile
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

# SymbolFile/Breakpad
SRCS=	SymbolFileBreakpad.cpp

# SymbolFile/DWARF
SRCS+=	AppleDWARFIndex.cpp \
	DebugNamesDWARFIndex.cpp \
	DIERef.cpp \
	DWARFAbbreviationDeclaration.cpp \
	DWARFASTParser.cpp \
	DWARFASTParserClang.cpp \
	DWARFAttribute.cpp \
	DWARFBaseDIE.cpp \
	DWARFCompileUnit.cpp \
	DWARFContext.cpp \
	DWARFDataExtractor.cpp \
	DWARFDebugAbbrev.cpp \
	DWARFDebugAranges.cpp \
	DWARFDebugArangeSet.cpp \
	DWARFDebugInfo.cpp \
	DWARFDebugInfoEntry.cpp \
	DWARFDebugMacro.cpp \
	DWARFDebugRanges.cpp \
	DWARFDeclContext.cpp \
	DWARFDefines.cpp \
	DWARFDIE.cpp \
	DWARFFormValue.cpp \
	DWARFIndex.cpp \
	DWARFTypeUnit.cpp \
	DWARFUnit.cpp \
	HashedNameToDIE.cpp \
	LogChannelDWARF.cpp \
	ManualDWARFIndex.cpp \
	NameToDIE.cpp \
	SymbolFileDWARF.cpp \
	SymbolFileDWARFDwo.cpp \
	SymbolFileDWARFDebugMap.cpp \
	UniqueDWARFASTType.cpp

# SymbolFile/NativePDB
SRCS+=	CodeViewRegisterMapping.cpp \
	CompileUnitIndex.cpp \
	DWARFLocationExpression.cpp \
	PdbAstBuilder.cpp \
	PdbFPOProgramToDWARFExpression.cpp \
	PdbIndex.cpp \
	PdbSymUid.cpp \
	PdbUtil.cpp \
	SymbolFileNativePDB.cpp \
	UdtRecordCompleter.cpp

# SymbolFile/PDB
SRCS+=	PDBASTParser.cpp \
	PDBLocationToDWARFExpression.cpp \
	SymbolFilePDB.cpp

# SymbolFile/Symtab
SRCS+=	SymbolFileSymtab.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/SymbolFile/Breakpad
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/SymbolFile/DWARF
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/SymbolFile/NativePDB
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/SymbolFile/PDB
.PATH:	${.CURDIR}/../../../llvm/lldb/source/Plugins/SymbolFile/Symtab

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${.OBJDIR}/../include/lldb/Plugins
