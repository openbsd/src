# $OpenBSD: Makefile,v 1.5 2020/08/03 14:45:26 patrick Exp $

LIB=	LLVMInstCombine
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms
CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms/InstCombine
CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Transforms/InstCombine

SRCS+=	AggressiveInstCombine.cpp \
	InstructionCombining.cpp \
	InstCombineAddSub.cpp \
	InstCombineAtomicRMW.cpp \
	InstCombineAndOrXor.cpp \
	InstCombineCalls.cpp \
	InstCombineCasts.cpp \
	InstCombineCompares.cpp \
	InstCombineLoadStoreAlloca.cpp \
	InstCombineMulDivRem.cpp \
	InstCombinePHI.cpp \
	InstCombineSelect.cpp \
	InstCombineShifts.cpp \
	InstCombineSimplifyDemanded.cpp \
	InstCombineVectorOps.cpp \
	TruncInstCombine.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Transforms/InstCombine
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Transforms/AggressiveInstCombine
