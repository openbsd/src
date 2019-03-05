# $OpenBSD: Makefile,v 1.3 2019/03/05 09:44:41 jsg Exp $

LIB=	LLVMMipsDisassembler
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Mips -I${LLVM_SRCS}/lib/Target/Mips

SRCS+=	MipsDisassembler.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/Mips/Disassembler
