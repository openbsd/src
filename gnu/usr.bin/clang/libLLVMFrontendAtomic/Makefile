# $OpenBSD: Makefile,v 1.1 2026/05/29 11:18:30 robert Exp $

LIB=	LLVMFrontendAtomic
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Frontend

SRCS+=	FrontendAtomic.cpp

FrontendAtomic.cpp:
	ln -s ${LLVM_SRCS}/lib/Frontend/Atomic/Atomic.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Frontend/Atomic

CLEANFILES+=	FrontendAtomic.cpp
