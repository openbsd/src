# $OpenBSD: Makefile,v 1.4 2019/02/26 23:39:51 jsg Exp $

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

.PATH:	${.CURDIR}/../../../llvm/lib/TableGen

install:
	@# Nothing here so far ...

CLEANFILES+= TableGenError.cpp

.include <bsd.lib.mk>
