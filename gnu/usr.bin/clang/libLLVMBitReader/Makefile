# $OpenBSD: Makefile,v 1.4 2019/03/05 09:44:40 jsg Exp $

LIB=	LLVMBitReader
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Bitcode

SRCS+=	BitReader.cpp \
	BitcodeReader.cpp \
	BitstreamReader.cpp \
	MetadataLoader.cpp \
	ValueList.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Bitcode/Reader
