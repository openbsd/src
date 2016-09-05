# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:49 pascal Exp $

LIB=	LLVMAsmParser
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Analysis

.include <bsd.own.mk>
SRCS=	LLLexer.cpp \
	LLParser.cpp \
	Parser.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/AsmParser

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
