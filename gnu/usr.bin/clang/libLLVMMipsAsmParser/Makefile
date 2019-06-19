# $OpenBSD: Makefile,v 1.3 2019/03/05 09:44:41 jsg Exp $

LIB=	LLVMMipsAsmParser
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Mips \
		-I${LLVM_SRCS}/lib/Target/Mips

SRCS+=	MipsAsmParser.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/Mips/AsmParser
