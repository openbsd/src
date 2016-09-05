# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:49 pascal Exp $

LIB=	LLVMLinker
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Linker

.include <bsd.own.mk>
SRCS=	IRMover.cpp \
	LinkModules.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Linker

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
