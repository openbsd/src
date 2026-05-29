# $OpenBSD: Makefile,v 1.10 2026/05/29 11:18:30 robert Exp $

LIB=	LLVMLTO
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/LTO

SRCS+=	LTO.cpp \
	LTOBackend.cpp \
	LTOModule.cpp \
	LTOCodeGenerator.cpp \
	UpdateCompilerUsed.cpp \
	ThinLTOCodeGenerator.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/LTO
