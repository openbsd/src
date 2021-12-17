# $OpenBSD: Makefile,v 1.3 2021/12/17 14:55:45 patrick Exp $

LIB=	LLVMFrontendOpenMP
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Frontend
CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Frontend/OpenMP

SRCS+=	OMP.cpp \
	OMPContext.cpp \
	OMPIRBuilder.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Frontend/OpenMP
