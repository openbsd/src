# $OpenBSD: Makefile,v 1.6 2023/11/11 18:35:37 robert Exp $

LIB=	LLVMPasses
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Passes \
		-I${LLVM_SRCS}/include/llvm/

SRCS+=	OptimizationLevel.cpp \
	PassBuilder.cpp \
	PassBuilderBindings.cpp \
	PassBuilderPipelines.cpp \
	PassPlugin.cpp \
	StandardInstrumentations.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Passes
