# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMIRReader
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/IRReader

.include <bsd.own.mk>
SRCS=	IRReader.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/IRReader

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
