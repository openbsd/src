# $OpenBSD: Makefile,v 1.3 2019/03/05 09:44:41 jsg Exp $

LIB=	LLVMMCParser
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/MC/MCParser

SRCS+=	AsmLexer.cpp \
	AsmParser.cpp \
	COFFAsmParser.cpp \
	DarwinAsmParser.cpp \
	ELFAsmParser.cpp \
	MCAsmLexer.cpp \
	MCAsmParser.cpp \
	MCAsmParserExtension.cpp \
	MCTargetAsmParser.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/MC/MCParser
