# $OpenBSD: Makefile,v 1.2 2026/05/29 11:18:30 robert Exp $

LIB=	LLVMFrontendOffloading
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Frontend

SRCS+=	Utility.cpp \
	OffloadWrapper.cpp \
	PropertySet.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Frontend/Offloading
