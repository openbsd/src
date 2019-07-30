# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:34 espie Exp $

LIB=	LLVMBitWriter
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	BitWriter.cpp \
	BitcodeWriter.cpp \
	BitcodeWriterPass.cpp \
	ValueEnumerator.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Bitcode/Writer

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
