# $OpenBSD: Makefile,v 1.4 2017/07/09 15:28:35 espie Exp $

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
	MachOObjectFile.cpp \
	MachOUniversal.cpp \
	ModuleSummaryIndexObjectFile.cpp \
	ModuleSymbolTable.cpp \
	Object.cpp \
	ObjectFile.cpp \
	RecordStreamer.cpp \
	SymbolicFile.cpp \
	SymbolSize.cpp \
	WasmObjectFile.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Object

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
