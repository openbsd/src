# $OpenBSD: Makefile,v 1.3 2017/10/04 20:57:09 patrick Exp $

LIB=	LLVMDebugInfoMSF
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/DebugInfo/MSF

.include <bsd.own.mk>
SRCS=	MappedBlockStream.cpp \
	MSFBuilder.cpp \
	MSFCommon.cpp \
	MSFError.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/DebugInfo/MSF

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
