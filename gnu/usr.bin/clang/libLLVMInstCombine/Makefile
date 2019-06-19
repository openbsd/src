# $OpenBSD: Makefile,v 1.4 2019/03/05 09:44:40 jsg Exp $

LIB=	LLVMInstCombine
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms
CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms/InstCombine
CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Transforms/InstCombine

SRCS+=	AggressiveInstCombine.cpp \
	InstructionCombining.cpp \
	InstCombineAddSub.cpp \
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

.PATH:	${.CURDIR}/../../../llvm/lib/Transforms/InstCombine
.PATH:	${.CURDIR}/../../../llvm/lib/Transforms/AggressiveInstCombine
