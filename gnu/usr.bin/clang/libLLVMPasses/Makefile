# $OpenBSD: Makefile,v 1.7 2025/06/11 17:46:32 robert Exp $

LIB=	LLVMPasses
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Passes \
		-I${LLVM_SRCS}/include/llvm/

SRCS+=	CodeGenPassBuilder.cpp \
	OptimizationLevel.cpp \
	PassBuilder.cpp \
	PassBuilderBindings.cpp \
	PassBuilderPipelines.cpp \
	PassPlugin.cpp \
	StandardInstrumentations.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Passes
