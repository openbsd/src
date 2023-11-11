# $OpenBSD: Makefile,v 1.1 2023/11/11 18:35:37 robert Exp $

LIB=	LLVMFrontendHLSL
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Frontend
CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Frontend/HLSL

SRCS+=	HLSLResource.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Frontend/HLSL
