# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:26 patrick Exp $

LIB=	LLVMMipsDisassembler
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Mips -I${LLVM_SRCS}/lib/Target/Mips

SRCS+=	MipsDisassembler.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/Mips/Disassembler
