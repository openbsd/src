# $OpenBSD: Makefile,v 1.8 2026/05/29 11:18:30 robert Exp $

LIB=	LLVMBinaryFormat
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/BinaryFormat

SRCS+=	AMDGPUMetadataVerifier.cpp \
	COFF.cpp \
	Dwarf.cpp \
	DXContainer.cpp \
	ELF.cpp \
	MachO.cpp \
	Magic.cpp \
	MsgPackDocument.cpp \
	MsgPackDocumentYAML.cpp \
	MsgPackReader.cpp \
	MsgPackWriter.cpp \
	SFrame.cpp \
	Wasm.cpp \
	XCOFF.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/BinaryFormat
