# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMDebugInfoMSF
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/DebugInfo/MSF

.include <bsd.own.mk>
SRCS=	MappedBlockStream.cpp \
	MSFBuilder.cpp \
	MSFCommon.cpp \
	MSFError.cpp \
	StreamReader.cpp \
	StreamWriter.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/DebugInfo/MSF

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
