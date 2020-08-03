# $OpenBSD: Makefile,v 1.5 2020/08/03 14:45:25 patrick Exp $

LIB=	LLVMDebugInfoPDB
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/DebugInfo/PDB

SRCS+=	DbiModuleDescriptor.cpp \
	DbiModuleDescriptorBuilder.cpp \
	DbiModuleList.cpp \
	DbiStream.cpp \
	DbiStreamBuilder.cpp \
	DebugInfoPDBEnumTables.cpp \
	GSIStreamBuilder.cpp \
	GenericError.cpp \
	GlobalsStream.cpp \
	Hash.cpp \
	HashTable.cpp \
	IPDBSourceFile.cpp \
	InfoStream.cpp \
	InfoStreamBuilder.cpp \
	InjectedSourceStream.cpp \
	ModuleDebugStream.cpp \
	NamedStreamMap.cpp \
	NativeCompilandSymbol.cpp \
	NativeEnumGlobals.cpp \
	NativeEnumInjectedSources.cpp \
	NativeEnumModules.cpp \
	NativeEnumTypes.cpp \
	NativeExeSymbol.cpp \
	NativeRawSymbol.cpp \
	NativeSymbolEnumerator.cpp \
	NativeTypeArray.cpp \
	NativeTypeBuiltin.cpp \
	NativeTypeEnum.cpp \
	NativeTypeFunctionSig.cpp \
	NativeTypePointer.cpp \
	NativeTypeTypedef.cpp \
	NativeTypeUDT.cpp \
	NativeTypeVTShape.cpp \
	NativeSession.cpp \
	PDB.cpp \
	PDBContext.cpp \
	PDBExtras.cpp \
	PDBFile.cpp \
	PDBFileBuilder.cpp \
	PDBInterfaceAnchors.cpp \
	PDBStringTable.cpp \
	PDBStringTableBuilder.cpp \
	PDBSymDumper.cpp \
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
	PublicsStream.cpp \
	RawError.cpp \
	SymbolCache.cpp \
	SymbolStream.cpp \
	TpiHashing.cpp \
	TpiStream.cpp \
	TpiStreamBuilder.cpp \
	UDTLayout.cpp \

DebugInfoPDBEnumTables.cpp:
	ln -s ${LLVM_SRCS}/lib/DebugInfo/PDB/Native/EnumTables.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/DebugInfo/PDB
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/DebugInfo/PDB/Native

CLEANFILES+= DebugInfoPDBEnumTables.cpp
