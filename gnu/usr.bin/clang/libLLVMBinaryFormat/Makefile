# $OpenBSD: Makefile,v 1.2 2019/01/27 16:59:41 patrick Exp $

LIB=	LLVMBinaryFormat
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/BinaryFormat

.include <bsd.own.mk>
SRCS=	Dwarf.cpp \
	Magic.cpp \
	Wasm.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/BinaryFormat

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
