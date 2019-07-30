# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:34 espie Exp $

LIB=	LLVMAArch64Disassembler
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AArch64 \
		-I${LLVM_SRCS}/lib/Target/AArch64

.include <bsd.own.mk>
SRCS=	AArch64Disassembler.cpp \
	AArch64ExternalSymbolizer.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/AArch64/Disassembler

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
