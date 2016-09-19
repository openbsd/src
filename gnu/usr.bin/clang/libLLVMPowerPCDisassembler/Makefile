# $OpenBSD: Makefile,v 1.1 2016/09/19 11:39:05 pascal Exp $

LIB=	LLVMPowerPCDisassembler
NOPIC=
NOPROFILE=

CPPFLAGS+=      -I${.OBJDIR}/../include/llvm/PowerPC \
		-I${LLVM_SRCS}/lib/Target/PowerPC

.include <bsd.own.mk>
SRCS=	PPCDisassembler.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/PowerPC/Disassembler

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
