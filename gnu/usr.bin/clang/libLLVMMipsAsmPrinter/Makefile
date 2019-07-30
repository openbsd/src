# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMMipsAsmPrinter
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Mips -I${LLVM_SRCS}/lib/Target/Mips

.include <bsd.own.mk>
SRCS=	MipsInstPrinter.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/Mips/InstPrinter

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
