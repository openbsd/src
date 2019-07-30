# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMMipsDisassembler
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Mips -I${LLVM_SRCS}/lib/Target/Mips

.include <bsd.own.mk>
SRCS=	MipsDisassembler.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/Mips/Disassembler

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
