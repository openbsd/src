# $OpenBSD: Makefile,v 1.1 2021/08/02 08:50:52 patrick Exp $

LIB=	LLVMSymbolize
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/DebugInfo/Symbolize

SRCS+=	DIPrinter.cpp \
	SymbolizableObjectFile.cpp \
	Symbolize.cpp \

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/DebugInfo/Symbolize
