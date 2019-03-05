# $OpenBSD: Makefile,v 1.3 2019/03/05 09:44:40 jsg Exp $

LIB=	LLVMBinaryFormat
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/BinaryFormat

SRCS+=	Dwarf.cpp \
	Magic.cpp \
	Wasm.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/BinaryFormat
