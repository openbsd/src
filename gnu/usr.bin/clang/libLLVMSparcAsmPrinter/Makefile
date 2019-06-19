# $OpenBSD: Makefile,v 1.3 2019/03/05 09:44:42 jsg Exp $

LIB=	LLVMSparcAsmPrinter
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Sparc \
		-I${LLVM_SRCS}/lib/Target/Sparc

SRCS+=	SparcInstPrinter.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/Sparc/InstPrinter
