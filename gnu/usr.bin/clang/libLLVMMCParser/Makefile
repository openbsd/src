# $OpenBSD: Makefile,v 1.7 2021/12/17 14:55:45 patrick Exp $

LIB=	LLVMMCParser
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/MC/MCParser

SRCS+=	AsmLexer.cpp \
	AsmParser.cpp \
	COFFAsmParser.cpp \
	COFFMasmParser.cpp \
	DarwinAsmParser.cpp \
	ELFAsmParser.cpp \
	MCAsmLexer.cpp \
	MCAsmParser.cpp \
	MCAsmParserExtension.cpp \
	MCTargetAsmParser.cpp \
	MasmParser.cpp \
	WasmAsmParser.cpp \
	XCOFFAsmParser.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/MC/MCParser
