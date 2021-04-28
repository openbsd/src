# $OpenBSD: Makefile,v 1.5 2021/04/28 12:55:39 patrick Exp $

LIB=	LLVMPasses
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Passes \
	-I${LLVM_SRCS}/include/llvm/

SRCS+=	PassBuilder.cpp \
	PassPlugin.cpp \
	StandardInstrumentations.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Passes
