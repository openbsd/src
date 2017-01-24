# $OpenBSD: Makefile,v 1.1 2017/01/24 08:44:47 patrick Exp $

LIB=	LLVMDemangle
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/Demangle

.include <bsd.own.mk>
SRCS=	ItaniumDemangle.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Demangle

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
