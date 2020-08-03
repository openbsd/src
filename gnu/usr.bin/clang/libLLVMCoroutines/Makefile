# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:25 patrick Exp $

LIB=	LLVMCoroutines
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms
CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms/Coroutines

SRCS+=	Coroutines.cpp \
	CoroCleanup.cpp \
	CoroEarly.cpp \
	CoroElide.cpp \
	CoroFrame.cpp \
	CoroSplit.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Transforms/Coroutines
