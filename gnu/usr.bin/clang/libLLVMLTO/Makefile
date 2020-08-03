# $OpenBSD: Makefile,v 1.7 2020/08/03 14:45:26 patrick Exp $

LIB=	LLVMLTO
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/LTO

SRCS+=	Caching.cpp \
	LTO.cpp \
	LTOBackend.cpp \
	LTOCodeGenerator.cpp \
	LTOModule.cpp \
	SummaryBasedOptimizations.cpp \
	UpdateCompilerUsed.cpp \
	ThinLTOCodeGenerator.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/LTO
