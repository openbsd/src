# $OpenBSD: Makefile,v 1.10 2021/12/17 14:55:45 patrick Exp $

LIB=	LLVMObject
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Object

SRCS+=	Archive.cpp \
	ArchiveWriter.cpp \
	Binary.cpp \
	COFFImportFile.cpp \
	COFFModuleDefinition.cpp \
	COFFObjectFile.cpp \
	Decompressor.cpp \
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
	RecordStreamer.cpp \
	RelocationResolver.cpp \
	SymbolicFile.cpp \
	SymbolSize.cpp \
	TapiFile.cpp \
	TapiUniversal.cpp \
	MachOUniversalWriter.cpp \
	WasmObjectFile.cpp \
	WindowsMachineFlag.cpp \
	WindowsResource.cpp \
	XCOFFObjectFile.cpp

ObjectELF.cpp:
	ln -s ${LLVM_SRCS}/lib/Object/ELF.cpp $@

ObjectError.cpp:
	ln -s ${LLVM_SRCS}/lib/Object/Error.cpp $@

ObjectMinidump.cpp:
	ln -s ${LLVM_SRCS}/lib/Object/Minidump.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Object

CLEANFILES+= ObjectELF.cpp ObjectError.cpp ObjectMinidump.cpp
