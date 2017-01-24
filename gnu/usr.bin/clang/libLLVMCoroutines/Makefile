# $OpenBSD: Makefile,v 1.1 2017/01/24 08:44:47 patrick Exp $

LIB=	LLVMCoroutines
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms
CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms/Coroutines

.include <bsd.own.mk>
SRCS=	Coroutines.cpp \
	CoroCleanup.cpp \
	CoroEarly.cpp \
	CoroElide.cpp \
	CoroFrame.cpp \
	CoroSplit.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Transforms/Coroutines

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
