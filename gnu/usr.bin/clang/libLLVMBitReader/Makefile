# $OpenBSD: Makefile,v 1.5 2020/08/03 14:45:25 patrick Exp $

LIB=	LLVMBitReader
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Bitcode

SRCS+=	BitcodeAnalyzer.cpp\
	BitReader.cpp \
	BitcodeReader.cpp \
	MetadataLoader.cpp \
	ValueList.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Bitcode/Reader
