# $OpenBSD: Makefile,v 1.1 2017/01/22 03:13:07 patrick Exp $

LIB=	LLVMPasses
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Passes

.include <bsd.own.mk>
SRCS=	PassBuilder.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Passes

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
