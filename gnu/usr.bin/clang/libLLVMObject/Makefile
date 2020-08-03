# $OpenBSD: Makefile,v 1.8 2020/08/03 14:45:26 patrick Exp $

LIB=	LLVMObject
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Object

SRCS+=	Archive.cpp \
	ArchiveWriter.cpp \
	Binary.cpp \
	COFFObjectFile.cpp \
	Decompressor.cpp \
	ELF.cpp \
	ELFObjectFile.cpp \
	ObjectError.cpp \
	ObjectMinidump.cpp \
	IRObjectFile.cpp \
	IRSymtab.cpp \
	MachOObjectFile.cpp \
	MachOUniversal.cpp \
	ModuleSymbolTable.cpp \
	Object.cpp \
	ObjectFile.cpp \
	RecordStreamer.cpp \
	RelocationResolver.cpp \
	SymbolicFile.cpp \
	SymbolSize.cpp \
	TapiFile.cpp \
	TapiUniversal.cpp \
	WasmObjectFile.cpp \
	WindowsMachineFlag.cpp \
	WindowsResource.cpp \
	XCOFFObjectFile.cpp

ObjectError.cpp:
	ln -s ${LLVM_SRCS}/lib/Object/Error.cpp $@

ObjectMinidump.cpp:
	ln -s ${LLVM_SRCS}/lib/Object/Minidump.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Object

CLEANFILES+= ObjectError.cpp ObjectMinidump.cpp
