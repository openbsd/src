# $OpenBSD: Makefile,v 1.1 2023/11/11 18:35:38 robert Exp $

LIB=	clangASTMatchers
NOPIC=
NOPROFILE=

#CPPFLAGS+=	-I${.OBJDIR}/../include/clang/AST \
CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>

SRCS=	ASTMatchFinder.cpp \
	ASTMatchersInternal.cpp \
	GtestMatchers.cpp

SRCS+=	Diagnostics.cpp \
	Marshallers.cpp \
	Parser.cpp \
	Registry.cpp \
	VariantValue.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/ASTMatchers
.PATH:	${.CURDIR}/../../../llvm/clang/lib/ASTMatchers/Dynamic

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
