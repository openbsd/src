# $OpenBSD: Makefile,v 1.9 2023/11/11 18:35:37 robert Exp $

LIB=	LLVMLTO
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/LTO

SRCS+=	LTO.cpp \
	LTOBackend.cpp \
	LTOModule.cpp \
	LTOCodeGenerator.cpp \
	SummaryBasedOptimizations.cpp \
	UpdateCompilerUsed.cpp \
	ThinLTOCodeGenerator.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/LTO
