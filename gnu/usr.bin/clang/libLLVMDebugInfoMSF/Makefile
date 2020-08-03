# $OpenBSD: Makefile,v 1.5 2020/08/03 14:45:25 patrick Exp $

LIB=	LLVMDebugInfoMSF
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/DebugInfo/MSF

SRCS+=	MappedBlockStream.cpp \
	MSFBuilder.cpp \
	MSFCommon.cpp \
	MSFError.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/DebugInfo/MSF
