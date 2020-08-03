# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:26 patrick Exp $

LIB=	LLVMLinker
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Linker

SRCS+=	IRMover.cpp \
	LinkModules.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Linker
