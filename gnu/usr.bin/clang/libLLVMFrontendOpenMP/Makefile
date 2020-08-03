# $OpenBSD: Makefile,v 1.1 2020/08/03 14:45:25 patrick Exp $

LIB=	LLVMFrontendOpenMP
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Frontend
CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Frontend/OpenMP

SRCS+=	OMPConstants.cpp \
	OMPIRBuilder.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Frontend/OpenMP
