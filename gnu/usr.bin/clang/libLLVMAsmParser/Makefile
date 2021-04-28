# $OpenBSD: Makefile,v 1.5 2021/04/28 12:55:38 patrick Exp $

LIB=	LLVMAsmParser
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/ASMParser

SRCS+=	LLLexer.cpp \
	LLParser.cpp \
	Parser.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/AsmParser
