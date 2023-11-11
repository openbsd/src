# $OpenBSD: Makefile,v 1.8 2023/11/11 18:35:37 robert Exp $

LIB=	LLVMMCParser
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/MC/MCParser

SRCS+=	AsmLexer.cpp \
	AsmParser.cpp \
	COFFAsmParser.cpp \
	COFFMasmParser.cpp \
	GOFFAsmParser.cpp \
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
