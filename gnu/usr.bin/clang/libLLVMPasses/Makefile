# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:26 patrick Exp $

LIB=	LLVMPasses
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Passes

SRCS+=	PassBuilder.cpp \
	PassPlugin.cpp \
	StandardInstrumentations.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Passes
