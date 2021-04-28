# $OpenBSD: Makefile,v 1.6 2021/04/28 12:55:39 patrick Exp $

LIB=	LLVMInstCombine
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms
CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms/InstCombine
CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Transforms/InstCombine

SRCS+=	InstructionCombining.cpp \
	InstCombineAddSub.cpp \
	InstCombineAtomicRMW.cpp \
	InstCombineAndOrXor.cpp \
	InstCombineCalls.cpp \
	InstCombineCasts.cpp \
	InstCombineCompares.cpp \
	InstCombineLoadStoreAlloca.cpp \
	InstCombineMulDivRem.cpp \
	InstCombineNegator.cpp \
	InstCombinePHI.cpp \
	InstCombineSelect.cpp \
	InstCombineShifts.cpp \
	InstCombineSimplifyDemanded.cpp \
	InstCombineVectorOps.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Transforms/InstCombine
