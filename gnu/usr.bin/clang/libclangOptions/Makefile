# $OpenBSD: Makefile,v 1.1 2026/05/29 11:18:31 robert Exp $

LIB=	clangOptions
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>

SRCS=	DriverOptions.cpp \
	OptionUtils.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/Options

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
