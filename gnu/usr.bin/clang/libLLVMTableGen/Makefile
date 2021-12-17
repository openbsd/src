# $OpenBSD: Makefile,v 1.7 2021/12/17 14:55:46 patrick Exp $

LIB=	LLVMTableGen
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/TableGen

.include <bsd.own.mk>
SRCS=	DetailedRecordsBackend.cpp \
	TableGenError.cpp \
	JSONBackend.cpp \
	Main.cpp \
	Record.cpp \
	SetTheory.cpp \
	StringMatcher.cpp \
	TableGenBackend.cpp \
	TableGenBackendSkeleton.cpp \
	TGLexer.cpp \
	TGParser.cpp

TableGenError.cpp:
	ln -s ${LLVM_SRCS}/lib/TableGen/Error.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/TableGen

install:
	@# Nothing here so far ...

CLEANFILES+= TableGenError.cpp

.include <bsd.lib.mk>
