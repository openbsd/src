# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:49 pascal Exp $

LIB=	LLVMTableGen
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/TableGen

.include <bsd.own.mk>
SRCS=	Error.cpp \
	Main.cpp \
	Record.cpp \
	SetTheory.cpp \
	StringMatcher.cpp \
	TableGenBackend.cpp \
	TGLexer.cpp \
	TGParser.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/TableGen

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
