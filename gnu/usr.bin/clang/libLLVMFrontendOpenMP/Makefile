# $OpenBSD: Makefile,v 1.4 2026/05/29 11:18:30 robert Exp $

LIB=	LLVMFrontendOpenMP
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Frontend
CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Frontend/OpenMP

SRCS+=	OMP.cpp \
	OMPContext.cpp \
	OMPIRBuilder.cpp \
	DirectiveNameParser.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Frontend/OpenMP
