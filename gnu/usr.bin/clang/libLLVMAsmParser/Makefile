# $OpenBSD: Makefile,v 1.3 2019/03/05 09:44:40 jsg Exp $

LIB=	LLVMAsmParser
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Analysis

SRCS+=	LLLexer.cpp \
	LLParser.cpp \
	Parser.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/AsmParser
