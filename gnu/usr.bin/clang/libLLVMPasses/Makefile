# $OpenBSD: Makefile,v 1.8 2026/05/29 11:18:31 robert Exp $

LIB=	LLVMPasses
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Passes \
		-I${LLVM_SRCS}/include/llvm/

SRCS+=	CodeGenPassBuilder.cpp \
	OptimizationLevel.cpp \
	PassBuilder.cpp \
	PassBuilderBindings.cpp \
	PassBuilderPipelines.cpp \
	StandardInstrumentations.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Passes
