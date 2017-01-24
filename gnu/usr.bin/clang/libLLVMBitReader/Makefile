# $OpenBSD: Makefile,v 1.2 2017/01/24 08:44:47 patrick Exp $

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

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
