# $OpenBSD: Makefile,v 1.1 2025/06/11 17:46:33 robert Exp $

LIB=	clangAPINotes
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES} -I${.OBJDIR}/../libclangBasic

.include <bsd.own.mk>

SRCS=	APINotesManager.cpp \
	APINotesReader.cpp \
	APINotesTypes.cpp \
	APINotesWriter.cpp \
	APINotesYAMLCompiler.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/APINotes

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
