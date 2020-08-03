# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:27 patrick Exp $

LIB=	LLVMPowerPCAsmParser
NOPROFILE=

CPPFLAGS+=      -I${.OBJDIR}/../include/llvm/PowerPC \
		-I${LLVM_SRCS}/lib/Target/PowerPC

SRCS+=	PPCAsmParser.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/PowerPC/AsmParser
