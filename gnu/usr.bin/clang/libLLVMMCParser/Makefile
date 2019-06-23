# $OpenBSD: Makefile,v 1.4 2019/06/23 22:07:40 patrick Exp $

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
	MCTargetAsmParser.cpp \
	WasmAsmParser.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/MC/MCParser
