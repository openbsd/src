# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:26 patrick Exp $

LIB=	LLVMMipsAsmParser
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Mips \
		-I${LLVM_SRCS}/lib/Target/Mips

SRCS+=	MipsAsmParser.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/Mips/AsmParser
