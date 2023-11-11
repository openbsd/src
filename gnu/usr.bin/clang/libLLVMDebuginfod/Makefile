# $OpenBSD: Makefile,v 1.1 2023/11/11 18:35:37 robert Exp $

LIB=	LLVMDebuginfod
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Debuginfod

SRCS+=	BuildIDFetcher.cpp \
	Debuginfod.cpp \
	HTTPClient.cpp \
	HTTPServer.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Debuginfod
