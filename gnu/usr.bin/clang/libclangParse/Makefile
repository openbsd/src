# $OpenBSD: Makefile,v 1.3 2020/08/03 14:45:29 patrick Exp $

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
	ParseInit.cpp \
	ParseObjc.cpp \
	ParseOpenMP.cpp \
	ParsePragma.cpp \
	ParseStmt.cpp \
	ParseStmtAsm.cpp \
	ParseTemplate.cpp \
	ParseTentative.cpp \
	Parser.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/Parse

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
