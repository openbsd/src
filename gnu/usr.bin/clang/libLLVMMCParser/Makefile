# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:35 espie Exp $

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

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
