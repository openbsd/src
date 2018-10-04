# $OpenBSD: Makefile,v 1.1 2018/10/04 11:43:29 patrick Exp $

LIB=	lldbPluginSymbolFile
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	DIERef.cpp \
	DWARFASTParserClang.cpp \
	DWARFASTParserGo.cpp \
	DWARFASTParserJava.cpp \
	DWARFASTParserOCaml.cpp \
	DWARFAbbreviationDeclaration.cpp \
	DWARFAttribute.cpp \
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
	DWARFDebugPubnames.cpp \
	DWARFDebugPubnamesSet.cpp \
	DWARFDebugRanges.cpp \
	DWARFDeclContext.cpp \
	DWARFDefines.cpp \
	DWARFFormValue.cpp \
	HashedNameToDIE.cpp \
	LogChannelDWARF.cpp \
	NameToDIE.cpp \
	PDBASTParser.cpp \
	SymbolFileDWARF.cpp \
	SymbolFileDWARFDebugMap.cpp \
	SymbolFileDWARFDwo.cpp \
	SymbolFileDWARFDwoDwp.cpp \
	SymbolFileDWARFDwp.cpp \
	SymbolFilePDB.cpp \
	SymbolFileSymtab.cpp \
	UniqueDWARFASTType.cpp \

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/SymbolFile/DWARF
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/SymbolFile/PDB
.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Plugins/SymbolFile/Symtab

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
