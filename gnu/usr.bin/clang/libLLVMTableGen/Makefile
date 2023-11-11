# $OpenBSD: Makefile,v 1.8 2023/11/11 18:35:38 robert Exp $

LIB=	LLVMTableGen
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/TableGen

.include <bsd.own.mk>

SRCS=	DetailedRecordsBackend.cpp \
	TableGenError.cpp \
	JSONBackend.cpp \
	Main.cpp \
	Parser.cpp \
	Record.cpp \
	SetTheory.cpp \
	StringMatcher.cpp \
	TGLexer.cpp \
	TGParser.cpp \
	TableGenBackend.cpp \
	TableGenBackendSkeleton.cpp

TableGenError.cpp:
	ln -s ${LLVM_SRCS}/lib/TableGen/Error.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/TableGen

install:
	@# Nothing here so far ...

CLEANFILES+= TableGenError.cpp

.include <bsd.lib.mk>
