# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:50 pascal Exp $

LIB=	LLVMVectorize
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Transforms

.include <bsd.own.mk>
SRCS=	BBVectorize.cpp \
	Vectorize.cpp \
	LoopVectorize.cpp \
	SLPVectorizer.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Transforms/Vectorize

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
