# $OpenBSD: Makefile,v 1.3 2019/03/05 09:44:39 jsg Exp $

LIB=	LLVMARMAsmPrinter
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/ARM -I${LLVM_SRCS}/lib/Target/ARM

SRCS+=	ARMInstPrinter.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/ARM/InstPrinter
