# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:50 pascal Exp $

LIB=	clangLex
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>
SRCS=	HeaderMap.cpp \
	HeaderSearch.cpp \
	Lexer.cpp \
	LiteralSupport.cpp \
	MacroArgs.cpp \
	MacroInfo.cpp \
	ModuleMap.cpp \
	PPCaching.cpp \
	PPCallbacks.cpp \
	PPConditionalDirectiveRecord.cpp \
	PPDirectives.cpp \
	PPExpressions.cpp \
	PPLexerChange.cpp \
	PPMacroExpansion.cpp \
	PTHLexer.cpp \
	Pragma.cpp \
	PreprocessingRecord.cpp \
	Preprocessor.cpp \
	PreprocessorLexer.cpp \
	ScratchBuffer.cpp \
	TokenConcatenation.cpp \
	TokenLexer.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/clang/lib/Lex

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
