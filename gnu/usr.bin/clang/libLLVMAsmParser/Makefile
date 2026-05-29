# $OpenBSD: Makefile,v 1.6 2026/05/29 11:18:30 robert Exp $

LIB=	LLVMAsmParser
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/ASMParser

SRCS+=	AsmParserContext.cpp \
	LLLexer.cpp \
	LLParser.cpp \
	Parser.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/AsmParser
