# $OpenBSD: Makefile,v 1.3 2019/06/23 22:07:44 patrick Exp $

LIB=	lldbPluginSymbolFile
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	AppleDWARFIndex.cpp \
	CompileUnitIndex.cpp \
	DIERef.cpp \
	DWARFASTParserClang.cpp \
	DWARFAbbreviationDeclaration.cpp \
	DWARFAttribute.cpp \
	DWARFBaseDIE.cpp \
	DWARFCompileUnit.cpp \
	DWARFDIE.cpp \
	DWARFDIECollection.cpp \
	DWARFDataExtractor.cpp \
	DWARFDebugAbbrev.cpp \
	DWARFDebugArangeSet.cpp \
	DWARFDebugAranges.cpp \
	DWARFDebugInfo.cpp \
	DWARFDebugInfoEntry.cpp \
	DWARFDebugLine.cpp \
	DWARFDebugMacinfo.cpp \
	DWARFDebugMacinfoEntry.cpp \
	DWARFDebugMacro.cpp \
	DWARFDebugRanges.cpp \
	DWARFDeclContext.cpp \
	DWARFDefines.cpp \
	DWARFFormValue.cpp \
	DWARFIndex.cpp \
	DWARFLocationExpression.cpp \
	DWARFUnit.cpp \
	DebugNamesDWARFIndex.cpp \
	HashedNameToDIE.cpp \
	LogChannelDWARF.cpp \
	ManualDWARFIndex.cpp \
	NameToDIE.cpp \
	PDBASTParser.cpp \
	PDBLocationToDWARFExpression.cpp \
	PdbAstBuilder.cpp \
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

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/SymbolFile/Breakpad
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/SymbolFile/DWARF
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/SymbolFile/NativePDB
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/SymbolFile/PDB
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/SymbolFile/Symtab

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
