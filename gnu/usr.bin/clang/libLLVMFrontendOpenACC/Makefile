# $OpenBSD: Makefile,v 1.1 2021/04/28 12:55:39 patrick Exp $

LIB=	LLVMFrontendOpenACC
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Frontend
CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Frontend/OpenACC

SRCS+=	ACC.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Frontend/OpenACC

ACC.cpp:
	ln -sf ${.OBJDIR}/../include/llvm/Frontend/OpenACC/ACC.cpp $@

CLEANFILES+= ACC.cpp

