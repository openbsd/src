# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:34 espie Exp $

LIB=	LLVMARMDisassembler
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/ARM -I${LLVM_SRCS}/lib/Target/ARM

.include <bsd.own.mk>
SRCS=	ARMDisassembler.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/ARM/Disassembler

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
