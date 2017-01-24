# $OpenBSD: Makefile,v 1.3 2017/01/24 08:44:47 patrick Exp $

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

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
