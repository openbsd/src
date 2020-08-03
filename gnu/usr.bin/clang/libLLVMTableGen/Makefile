# $OpenBSD: Makefile,v 1.5 2020/08/03 14:45:27 patrick Exp $

LIB=	LLVMTableGen
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/TableGen

.include <bsd.own.mk>
SRCS=	TableGenError.cpp \
	Main.cpp \
	JSONBackend.cpp \
	Record.cpp \
	SetTheory.cpp \
	StringMatcher.cpp \
	TableGenBackend.cpp \
	TGLexer.cpp \
	TGParser.cpp

TableGenError.cpp:
	ln -s ${LLVM_SRCS}/lib/TableGen/Error.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/TableGen

install:
	@# Nothing here so far ...

CLEANFILES+= TableGenError.cpp

.include <bsd.lib.mk>
