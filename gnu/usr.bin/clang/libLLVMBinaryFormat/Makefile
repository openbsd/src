# $OpenBSD: Makefile,v 1.7 2023/11/11 18:35:36 robert Exp $

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
	BinaryFormatMinidump.cpp \
	MsgPackDocument.cpp \
	MsgPackDocumentYAML.cpp \
	MsgPackReader.cpp \
	MsgPackWriter.cpp \
	Wasm.cpp \
	XCOFF.cpp

BinaryFormatMinidump.cpp:
	ln -s ${LLVM_SRCS}/lib/BinaryFormat/Minidump.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/BinaryFormat

CLEANFILES+= BinaryFormatMinidump.cpp \
