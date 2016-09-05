# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:49 pascal Exp $

LIB=	LLVMMIRParser
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	MILexer.cpp \
	MIParser.cpp \
	MIRParser.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/CodeGen/MIRParser

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
