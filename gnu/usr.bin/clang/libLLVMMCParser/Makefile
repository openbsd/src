# $OpenBSD: Makefile,v 1.5 2020/08/03 14:45:26 patrick Exp $

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

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/MC/MCParser
