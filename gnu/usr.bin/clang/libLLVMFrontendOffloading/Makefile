# $OpenBSD: Makefile,v 1.1 2025/06/11 17:46:32 robert Exp $

LIB=	LLVMFrontendOffloading
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Frontend

SRCS+=	Utility.cpp \
	OffloadWrapper.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Frontend/Offloading
