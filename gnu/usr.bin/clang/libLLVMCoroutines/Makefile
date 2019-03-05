# $OpenBSD: Makefile,v 1.3 2019/03/05 09:44:40 jsg Exp $

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

.PATH:	${.CURDIR}/../../../llvm/lib/Transforms/Coroutines
