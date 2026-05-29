# $OpenBSD: Makefile,v 1.1 2026/05/29 11:18:31 robert Exp $

LIB=	clangAnalysisLifetimeSafety
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>

SRCS=	Checker.cpp \
	Facts.cpp \
	FactsGenerator.cpp \
	LifetimeAnnotations.cpp \
	LifetimeSafety.cpp \
	LiveOrigins.cpp \
	LifetimeStats.cpp \
	Loans.cpp \
	LoanPropagation.cpp \
	Origins.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/Analysis/LifetimeSafety

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
