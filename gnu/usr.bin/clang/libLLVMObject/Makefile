# $OpenBSD: Makefile,v 1.5 2017/10/04 20:57:09 patrick Exp $

LIB=	LLVMObject
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Object

.include <bsd.own.mk>
SRCS=	Archive.cpp \
	ArchiveWriter.cpp \
	Binary.cpp \
	COFFObjectFile.cpp \
	Decompressor.cpp \
	ELF.cpp \
	ELFObjectFile.cpp \
	Error.cpp \
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

.PATH:	${.CURDIR}/../../../llvm/lib/Object

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
