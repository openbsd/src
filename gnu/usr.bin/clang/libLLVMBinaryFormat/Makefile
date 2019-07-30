# $OpenBSD: Makefile,v 1.1 2017/10/04 20:57:09 patrick Exp $

LIB=	LLVMBinaryFormat
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/BinaryFormat

.include <bsd.own.mk>
SRCS=	Dwarf.cpp \
	Magic.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/BinaryFormat

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
