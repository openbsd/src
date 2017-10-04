# $OpenBSD: Makefile,v 1.4 2017/10/04 20:57:09 patrick Exp $

LIB=	LLVMVectorize
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms

.include <bsd.own.mk>
SRCS=	Vectorize.cpp \
	LoadStoreVectorizer.cpp \
	LoopVectorize.cpp \
	SLPVectorizer.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Transforms/Vectorize

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
