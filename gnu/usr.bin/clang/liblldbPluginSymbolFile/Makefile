# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:31 patrick Exp $

LIB=	lldbPluginSymbolFile
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	AppleDWARFIndex.cpp \
	CodeViewRegisterMapping.cpp \
	CompileUnitIndex.cpp \
	DIERef.cpp \
	DWARFASTParserClang.cpp \
	DWARFAbbreviationDeclaration.cpp \
	DWARFAttribute.cpp \
	DWARFBaseDIE.cpp \
	DWARFCompileUnit.cpp \
	DWARFContext.cpp \
	DWARFDIE.cpp \
	DWARFDataExtractor.cpp \
	DWARFDebugAbbrev.cpp \
	DWARFDebugArangeSet.cpp \
	DWARFDebugAranges.cpp \
	DWARFDebugInfo.cpp \
	DWARFDebugInfoEntry.cpp \
	DWARFDebugMacro.cpp \
	DWARFDebugRanges.cpp \
	DWARFDeclContext.cpp \
	DWARFDefines.cpp \
	DWARFFormValue.cpp \
	DWARFIndex.cpp \
	DWARFLocationExpression.cpp \
	DWARFTypeUnit.cpp \
	DWARFUnit.cpp \
	DebugNamesDWARFIndex.cpp \
	HashedNameToDIE.cpp \
	LogChannelDWARF.cpp \
	ManualDWARFIndex.cpp \
	NameToDIE.cpp \
	PDBASTParser.cpp \
	PDBLocationToDWARFExpression.cpp \
	PdbAstBuilder.cpp \
	PdbFPOProgramToDWARFExpression.cpp \
	PdbIndex.cpp \
	PdbSymUid.cpp \
	PdbUtil.cpp \
	SymbolFileBreakpad.cpp \
	SymbolFileDWARF.cpp \
	SymbolFileDWARFDebugMap.cpp \
	SymbolFileDWARFDwo.cpp \
	SymbolFileDWARFDwoDwp.cpp \
	SymbolFileDWARFDwp.cpp \
	SymbolFileNativePDB.cpp \
	SymbolFilePDB.cpp \
	SymbolFileSymtab.cpp \
	UdtRecordCompleter.cpp \
	UniqueDWARFASTType.cpp \

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
