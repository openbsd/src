# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:27 patrick Exp $

LIB=	LLVMSparcDisassembler
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Sparc \
		-I${LLVM_SRCS}/lib/Target/Sparc

SRCS+=	SparcDisassembler.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/Sparc/Disassembler
