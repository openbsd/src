# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMX86Disassembler
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/X86 -I${LLVM_SRCS}/lib/Target/X86

.include <bsd.own.mk>
SRCS=	X86Disassembler.cpp \
	X86DisassemblerDecoder.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/X86/Disassembler

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
