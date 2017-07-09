# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMPasses
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Passes

.include <bsd.own.mk>
SRCS=	PassBuilder.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Passes

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
