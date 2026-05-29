# $OpenBSD: Makefile,v 1.6 2026/05/29 11:18:30 robert Exp $

LIB=	LLVMCoroutines
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms
CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms/Coroutines

SRCS+=	Coroutines.cpp \
	CoroAnnotationElide.cpp \
	CoroCleanup.cpp \
	CoroConditionalWrapper.cpp \
	CoroEarly.cpp \
	CoroElide.cpp \
	CoroFrame.cpp \
	CoroSplit.cpp \
	SuspendCrossingInfo.cpp \
	SpillUtils.cpp \
	MaterializationUtils.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Transforms/Coroutines
