# $OpenBSD: Makefile,v 1.2 2021/12/17 14:55:45 patrick Exp $

LIB=	LLVMFrontendOpenACC
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Frontend
CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Frontend/OpenACC

SRCS+=	ACC.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Frontend/OpenACC
