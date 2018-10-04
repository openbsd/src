# $OpenBSD: Makefile,v 1.1 2018/10/04 11:43:29 patrick Exp $

LIB=	LLVMDebugInfoPDB
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/DebugInfo/PDB

.include <bsd.own.mk>
SRCS=	DbiModuleDescriptor.cpp \
	DbiModuleDescriptorBuilder.cpp \
	DbiModuleList.cpp \
	DbiStream.cpp \
	DbiStreamBuilder.cpp \
	EnumTables.cpp \
	GSIStreamBuilder.cpp \
	GenericError.cpp \
	GlobalsStream.cpp \
	Hash.cpp \
	HashTable.cpp \
	IPDBSourceFile.cpp \
	InfoStream.cpp \
	InfoStreamBuilder.cpp \
	ModuleDebugStream.cpp \
	NamedStreamMap.cpp \
	NativeBuiltinSymbol.cpp \
	NativeCompilandSymbol.cpp \
	NativeEnumModules.cpp \
	NativeEnumSymbol.cpp \
	NativeEnumTypes.cpp \
	NativeExeSymbol.cpp \
	NativeRawSymbol.cpp \
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
	SymbolStream.cpp \
	TpiHashing.cpp \
	TpiStream.cpp \
	TpiStreamBuilder.cpp \
	UDTLayout.cpp \

.PATH:	${.CURDIR}/../../../llvm/lib/DebugInfo/PDB
.PATH:	${.CURDIR}/../../../llvm/lib/DebugInfo/PDB/Native

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
