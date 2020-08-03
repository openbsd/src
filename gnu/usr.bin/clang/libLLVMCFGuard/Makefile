# $OpenBSD: Makefile,v 1.1 2020/08/03 14:45:25 patrick Exp $

LIB=	LLVMCFGuard
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms

SRCS+=	CFGuard.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Transforms/CFGuard
