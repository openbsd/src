# $OpenBSD: Makefile,v 1.3 2019/03/05 09:44:39 jsg Exp $

LIB=	LLVMARMAsmParser
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/ARM -I${LLVM_SRCS}/lib/Target/ARM

SRCS+=	ARMAsmParser.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/ARM/AsmParser
