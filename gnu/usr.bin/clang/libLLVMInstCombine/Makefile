# $OpenBSD: Makefile,v 1.3 2019/01/27 16:59:41 patrick Exp $

LIB=	LLVMInstCombine
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms
CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms/InstCombine
CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Transforms/InstCombine

.include <bsd.own.mk>
SRCS=	AggressiveInstCombine.cpp \
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

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
