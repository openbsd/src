# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:29 patrick Exp $

LIB=	clangLex
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>
SRCS=	DependencyDirectivesSourceMinimizer.cpp \
	HeaderMap.cpp \
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
	Pragma.cpp \
	PreprocessingRecord.cpp \
	Preprocessor.cpp \
	PreprocessorLexer.cpp \
	ScratchBuffer.cpp \
	TokenConcatenation.cpp \
	TokenLexer.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/Lex

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
