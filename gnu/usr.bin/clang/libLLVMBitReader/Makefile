# $OpenBSD: Makefile,v 1.3 2017/07/09 15:28:34 espie Exp $

LIB=	LLVMBitReader
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Bitcode

.include <bsd.own.mk>
SRCS=	BitReader.cpp \
	BitcodeReader.cpp \
	BitstreamReader.cpp \
	MetadataLoader.cpp \
	ValueList.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Bitcode/Reader

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
