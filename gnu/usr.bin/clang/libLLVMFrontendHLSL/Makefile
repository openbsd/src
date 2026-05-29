# $OpenBSD: Makefile,v 1.2 2026/05/29 11:18:30 robert Exp $

LIB=	LLVMFrontendHLSL
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Frontend
CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Frontend/HLSL

SRCS+=	CBuffer.cpp \
	HLSLBinding.cpp \
	HLSLResource.cpp \
	HLSLRootSignature.cpp \
	RootSignatureMetadata.cpp \
	RootSignatureValidations.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Frontend/HLSL
