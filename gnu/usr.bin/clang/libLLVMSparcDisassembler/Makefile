# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMSparcDisassembler
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Sparc \
		-I${LLVM_SRCS}/lib/Target/Sparc

.include <bsd.own.mk>
SRCS=	SparcDisassembler.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/Sparc/Disassembler

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
