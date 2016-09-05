# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:49 pascal Exp $

LIB=	LLVMBitReader
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Bitcode

.include <bsd.own.mk>
SRCS=	BitReader.cpp \
	BitcodeReader.cpp \
	BitstreamReader.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Bitcode/Reader

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
