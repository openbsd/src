# $OpenBSD: Makefile,v 1.3 2019/03/05 09:44:41 jsg Exp $

LIB=	LLVMPasses
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Passes

SRCS+=	PassBuilder.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Passes
