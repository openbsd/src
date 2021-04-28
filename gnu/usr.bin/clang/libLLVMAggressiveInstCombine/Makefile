# $OpenBSD: Makefile,v 1.1 2021/04/28 12:55:38 patrick Exp $

LIB=	LLVMAggressiveInstCombine
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms

SRCS+=	AggressiveInstCombine.cpp \
	TruncInstCombine.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Transforms/AggressiveInstCombine
