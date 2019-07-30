# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMLinker
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Linker

.include <bsd.own.mk>
SRCS=	IRMover.cpp \
	LinkModules.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Linker

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
