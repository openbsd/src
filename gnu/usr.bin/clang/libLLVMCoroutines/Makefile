# $OpenBSD: Makefile,v 1.5 2023/11/11 18:35:36 robert Exp $

LIB=	LLVMCoroutines
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms
CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms/Coroutines

SRCS+=	Coroutines.cpp \
	CoroCleanup.cpp \
	CoroConditionalWrapper.cpp \
	CoroEarly.cpp \
	CoroElide.cpp \
	CoroFrame.cpp \
	CoroSplit.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Transforms/Coroutines
