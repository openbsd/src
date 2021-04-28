# $OpenBSD: Makefile,v 1.5 2021/04/28 12:55:39 patrick Exp $

LIB=	LLVMMIRParser
NOPROFILE=

CPPFLAGS+=-I${LLVM_SRC}/include/llvm/CodeGen/MIRParser

SRCS+=	MILexer.cpp \
	MIParser.cpp \
	MIRParser.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/CodeGen/MIRParser
