# $OpenBSD: Makefile,v 1.2 2023/11/11 18:35:38 robert Exp $

LIB=	LLVMSymbolize
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/DebugInfo/Symbolize

SRCS+=	DIPrinter.cpp \
	Markup.cpp \
	MarkupFilter.cpp \
	SymbolizableObjectFile.cpp \
	Symbolize.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/DebugInfo/Symbolize
