# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:49 pascal Exp $

LIB=	LLVMMCParser
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/MC/MCParser

.include <bsd.own.mk>
SRCS=	AsmLexer.cpp \
	AsmParser.cpp \
	COFFAsmParser.cpp \
	DarwinAsmParser.cpp \
	ELFAsmParser.cpp \
	MCAsmLexer.cpp \
	MCAsmParser.cpp \
	MCAsmParserExtension.cpp \
	MCTargetAsmParser.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/MC/MCParser

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
