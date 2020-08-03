# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:28 patrick Exp $

LIB=	LLVMX86AsmParser
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/X86 -I${LLVM_SRCS}/lib/Target/X86

SRCS+=	X86AsmParser.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/X86/AsmParser
