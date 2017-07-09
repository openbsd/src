# $OpenBSD: Makefile,v 1.3 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMVectorize
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms

.include <bsd.own.mk>
SRCS=	BBVectorize.cpp \
	Vectorize.cpp \
	LoadStoreVectorizer.cpp \
	LoopVectorize.cpp \
	SLPVectorizer.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Transforms/Vectorize

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
