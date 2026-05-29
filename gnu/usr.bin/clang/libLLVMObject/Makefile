# $OpenBSD: Makefile,v 1.13 2026/05/29 11:18:31 robert Exp $

LIB=	LLVMObject
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Object

SRCS+=	Archive.cpp \
	ArchiveWriter.cpp \
	Binary.cpp \
	BuildID.cpp \
	COFFImportFile.cpp \
	COFFModuleDefinition.cpp \
	COFFObjectFile.cpp \
	Decompressor.cpp \
	ObjectDXContainer.cpp \
	ObjectELF.cpp \
	ELFObjectFile.cpp \
	ObjectError.cpp \
	FaultMapParser.cpp \
	IRObjectFile.cpp \
	IRSymtab.cpp \
	MachOObjectFile.cpp \
	MachOUniversal.cpp \
	ObjectMinidump.cpp \
	ModuleSymbolTable.cpp \
	Object.cpp \
	ObjectFile.cpp \
	OffloadBinary.cpp \
	OffloadBundle.cpp \
	RecordStreamer.cpp \
	RelocationResolver.cpp \
	SFrameParser.cpp \
	SymbolicFile.cpp \
	SymbolSize.cpp \
	TapiFile.cpp \
	TapiUniversal.cpp \
	MachOUniversalWriter.cpp \
	WasmObjectFile.cpp \
	WindowsMachineFlag.cpp \
	WindowsResource.cpp \
	XCOFFObjectFile.cpp

ObjectDXContainer.cpp:
	ln -s ${LLVM_SRCS}/lib/Object/DXContainer.cpp $@

ObjectELF.cpp:
	ln -s ${LLVM_SRCS}/lib/Object/ELF.cpp $@

ObjectError.cpp:
	ln -s ${LLVM_SRCS}/lib/Object/Error.cpp $@

ObjectMinidump.cpp:
	ln -s ${LLVM_SRCS}/lib/Object/Minidump.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Object

CLEANFILES+= ObjectDXContainer.cpp ObjectELF.cpp ObjectError.cpp \
	ObjectMinidump.cpp
