# $OpenBSD: Makefile,v 1.2 2017/01/14 20:12:41 patrick Exp $

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

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
