# $OpenBSD: Makefile,v 1.9 2026/05/29 11:18:30 robert Exp $

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
	MCAsmParser.cpp \
	MCAsmParserExtension.cpp \
	MCTargetAsmParser.cpp \
	MasmParser.cpp \
	WasmAsmParser.cpp \
	XCOFFAsmParser.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/MC/MCParser
