# $OpenBSD: Makefile,v 1.1 2020/08/03 14:45:25 patrick Exp $

LIB=	LLVMBitstreamReader
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Bitcode
CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Bitstream

SRCS+=	BitstreamReader.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Bitstream/Reader
