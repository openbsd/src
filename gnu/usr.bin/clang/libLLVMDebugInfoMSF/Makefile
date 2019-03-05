# $OpenBSD: Makefile,v 1.4 2019/03/05 09:44:40 jsg Exp $

LIB=	LLVMDebugInfoMSF
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/DebugInfo/MSF

SRCS+=	MappedBlockStream.cpp \
	MSFBuilder.cpp \
	MSFCommon.cpp \
	MSFError.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/DebugInfo/MSF
