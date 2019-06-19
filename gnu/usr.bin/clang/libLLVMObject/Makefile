# $OpenBSD: Makefile,v 1.7 2019/03/05 09:44:41 jsg Exp $

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
	IRObjectFile.cpp \
	IRSymtab.cpp \
	MachOObjectFile.cpp \
	MachOUniversal.cpp \
	ModuleSymbolTable.cpp \
	Object.cpp \
	ObjectFile.cpp \
	RecordStreamer.cpp \
	SymbolicFile.cpp \
	SymbolSize.cpp \
	WasmObjectFile.cpp \
	WindowsResource.cpp

ObjectError.cpp:
	ln -s ${LLVM_SRCS}/lib/Object/Error.cpp $@

.PATH:	${.CURDIR}/../../../llvm/lib/Object

CLEANFILES+= ObjectError.cpp
