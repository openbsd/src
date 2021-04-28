# $OpenBSD: Makefile,v 1.8 2021/04/28 12:55:39 patrick Exp $

LIB=	LLVMLTO
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/LTO

SRCS+=	Caching.cpp \
	LTO.cpp \
	LTOBackend.cpp \
	LTOModule.cpp \
	LTOCodeGenerator.cpp \
	SummaryBasedOptimizations.cpp \
	UpdateCompilerUsed.cpp \
	ThinLTOCodeGenerator.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/LTO
