# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMSparcAsmParser
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Sparc \
		-I${LLVM_SRCS}/lib/Target/Sparc

.include <bsd.own.mk>
SRCS=	SparcAsmParser.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/Sparc/AsmParser

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
