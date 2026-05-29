# $OpenBSD: Makefile,v 1.6 2026/05/29 11:18:31 robert Exp $

LIB=	clangParse
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>

SRCS=	ParseAST.cpp \
	ParseCXXInlineMethods.cpp \
	ParseDecl.cpp \
	ParseDeclCXX.cpp \
	ParseExpr.cpp \
	ParseExprCXX.cpp \
	ParseHLSL.cpp \
	ParseHLSLRootSignature.cpp \
	ParseInit.cpp \
	ParseObjc.cpp \
	ParseOpenMP.cpp \
	ParsePragma.cpp \
	ParseStmt.cpp \
	ParseStmtAsm.cpp \
	ParseTemplate.cpp \
	ParseTentative.cpp \
	Parser.cpp \
	ParseOpenACC.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/Parse

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
