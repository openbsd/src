# $OpenBSD: Makefile,v 1.3 2019/03/05 09:44:41 jsg Exp $

LIB=	LLVMSparcAsmParser
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Sparc \
		-I${LLVM_SRCS}/lib/Target/Sparc

SRCS+=	SparcAsmParser.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/Sparc/AsmParser
