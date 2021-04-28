# $OpenBSD: Makefile,v 1.6 2021/04/28 12:55:39 patrick Exp $

LIB=	LLVMBinaryFormat
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/BinaryFormat

SRCS+=	AMDGPUMetadataVerifier.cpp \
	Dwarf.cpp \
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

CLEANFILES+= BinaryFormatMinidump.cpp
