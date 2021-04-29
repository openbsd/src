# $OpenBSD: Makefile,v 1.1 2021/04/29 05:32:01 drahn Exp $

LIB=	LLVMRISCVAsmParser
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/RISCV \
		-I${LLVM_SRCS}/lib/Target/RISCV

SRCS+=	RISCVAsmParser.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/RISCV/AsmParser
