# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:49 pascal Exp $

LIB=	LLVMBitWriter
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	BitWriter.cpp \
	BitcodeWriter.cpp \
	BitcodeWriterPass.cpp \
	ValueEnumerator.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Bitcode/Writer

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
