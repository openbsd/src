# $OpenBSD: Makefile,v 1.4 2019/06/23 22:07:39 patrick Exp $

LIB=	LLVMBinaryFormat
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/BinaryFormat

SRCS+=	AMDGPUMetadataVerifier.cpp \
	Dwarf.cpp \
	Magic.cpp \
	MsgPackReader.cpp \
	MsgPackTypes.cpp \
	MsgPackWriter.cpp \
	Wasm.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/BinaryFormat
