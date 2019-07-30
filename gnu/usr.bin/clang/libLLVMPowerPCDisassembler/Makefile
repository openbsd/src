# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMPowerPCDisassembler
NOPIC=
NOPROFILE=

CPPFLAGS+=      -I${.OBJDIR}/../include/llvm/PowerPC \
		-I${LLVM_SRCS}/lib/Target/PowerPC

.include <bsd.own.mk>
SRCS=	PPCDisassembler.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/PowerPC/Disassembler

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
