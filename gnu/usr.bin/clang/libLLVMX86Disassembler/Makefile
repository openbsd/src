# $OpenBSD: Makefile,v 1.3 2019/03/05 09:44:42 jsg Exp $

LIB=	LLVMX86Disassembler
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/X86 -I${LLVM_SRCS}/lib/Target/X86

SRCS+=	X86Disassembler.cpp \
	X86DisassemblerDecoder.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/X86/Disassembler
