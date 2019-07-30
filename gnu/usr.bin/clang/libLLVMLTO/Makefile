# $OpenBSD: Makefile,v 1.4 2017/10/04 20:57:09 patrick Exp $

LIB=	LLVMLTO
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/LTO

.include <bsd.own.mk>
SRCS=	Caching.cpp \
	LTO.cpp \
	LTOBackend.cpp \
	LTOCodeGenerator.cpp \
	LTOModule.cpp \
	UpdateCompilerUsed.cpp \
	ThinLTOCodeGenerator.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/LTO

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
