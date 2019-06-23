# $OpenBSD: Makefile,v 1.6 2019/06/23 22:07:40 patrick Exp $

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

.PATH:	${.CURDIR}/../../../llvm/lib/LTO
