# $OpenBSD: Makefile,v 1.8 2023/11/11 18:35:37 robert Exp $

LIB=	LLVMDebugInfoPDB
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/DebugInfo/PDB

# DIA
#SRCS+=	DIADataStream.cpp \
#	DIAEnumDebugStreams.cpp \
#	DIAEnumFrameData.cpp \
#	DIAEnumInjectedSources.cpp \
#	DIAEnumLineNumbers.cpp \
#	DIAEnumSectionContribs.cpp \
#	DIAEnumSourceFiles.cpp \
#	DIAEnumSymbols.cpp \
#	DIAEnumTables.cpp \
#	DIAError.cpp \
#	DIAFrameData.cpp \
#	DIAInjectedSource.cpp \
#	DIALineNumber.cpp \
#	DIARawSymbol.cpp \
#	DIASectionContrib.cpp \
#	DIASession.cpp \
#	DIASourceFile.cpp \
#	DIATable.cpp

# Native
SRCS+=	DbiModuleDescriptor.cpp \
	DbiModuleDescriptorBuilder.cpp \
	DbiModuleList.cpp \
	DbiStream.cpp \
	DbiStreamBuilder.cpp \
	DebugInfoPDBEnumTables.cpp \
	FormatUtil.cpp \
	GlobalsStream.cpp \
	Hash.cpp \
	HashTable.cpp \
	InfoStream.cpp \
	InfoStreamBuilder.cpp \
	InjectedSourceStream.cpp \
	InputFile.cpp \
	LinePrinter.cpp \
	ModuleDebugStream.cpp \
	NativeCompilandSymbol.cpp \
	NativeEnumGlobals.cpp \
	NativeEnumInjectedSources.cpp \
	NativeEnumLineNumbers.cpp \
	NativeEnumModules.cpp \
	NativeEnumTypes.cpp \
	NativeEnumSymbols.cpp \
	NativeExeSymbol.cpp \
	NativeFunctionSymbol.cpp \
	NativeInlineSiteSymbol.cpp \
	NativeLineNumber.cpp \
	NativePublicSymbol.cpp \
	NativeRawSymbol.cpp \
	NativeSourceFile.cpp \
	NativeSymbolEnumerator.cpp \
	NativeTypeArray.cpp \
	NativeTypeBuiltin.cpp \
	NativeTypeEnum.cpp \
	NativeTypeFunctionSig.cpp \
	NativeTypePointer.cpp \
	NativeTypeTypedef.cpp \
	NativeTypeUDT.cpp \
	NativeTypeVTShape.cpp \
	NamedStreamMap.cpp \
	NativeSession.cpp \
	PDBFile.cpp \
	PDBFileBuilder.cpp \
	PDBStringTable.cpp \
	PDBStringTableBuilder.cpp \
	PublicsStream.cpp \
	GSIStreamBuilder.cpp \
	RawError.cpp \
	SymbolCache.cpp \
	SymbolStream.cpp \
	TpiHashing.cpp \
	TpiStream.cpp \
	TpiStreamBuilder.cpp

SRCS+=	GenericError.cpp \
	IPDBSourceFile.cpp \
	PDB.cpp \
	PDBContext.cpp \
	PDBExtras.cpp \
	PDBInterfaceAnchors.cpp \
	PDBSymbol.cpp \
	PDBSymbolAnnotation.cpp \
	PDBSymbolBlock.cpp \
	PDBSymbolCompiland.cpp \
	PDBSymbolCompilandDetails.cpp \
	PDBSymbolCompilandEnv.cpp \
	PDBSymbolCustom.cpp \
	PDBSymbolData.cpp \
	PDBSymbolExe.cpp \
	PDBSymbolFunc.cpp \
	PDBSymbolFuncDebugEnd.cpp \
	PDBSymbolFuncDebugStart.cpp \
	PDBSymbolLabel.cpp \
	PDBSymbolPublicSymbol.cpp \
	PDBSymbolThunk.cpp \
	PDBSymbolTypeArray.cpp \
	PDBSymbolTypeBaseClass.cpp \
	PDBSymbolTypeBuiltin.cpp \
	PDBSymbolTypeCustom.cpp \
	PDBSymbolTypeDimension.cpp \
	PDBSymbolTypeEnum.cpp \
	PDBSymbolTypeFriend.cpp \
	PDBSymbolTypeFunctionArg.cpp \
	PDBSymbolTypeFunctionSig.cpp \
	PDBSymbolTypeManaged.cpp \
	PDBSymbolTypePointer.cpp \
	PDBSymbolTypeTypedef.cpp \
	PDBSymbolTypeUDT.cpp \
	PDBSymbolTypeVTable.cpp \
	PDBSymbolTypeVTableShape.cpp \
	PDBSymbolUnknown.cpp \
	PDBSymbolUsingNamespace.cpp \
	PDBSymDumper.cpp \
	UDTLayout.cpp \

DebugInfoPDBEnumTables.cpp:
	ln -s ${LLVM_SRCS}/lib/DebugInfo/PDB/Native/EnumTables.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/DebugInfo/PDB
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/DebugInfo/PDB/Native
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/DebugInfo/PDB/DIA

CLEANFILES+= DebugInfoPDBEnumTables.cpp
