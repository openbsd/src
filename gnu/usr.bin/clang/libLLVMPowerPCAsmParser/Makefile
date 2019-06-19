# $OpenBSD: Makefile,v 1.3 2019/03/05 09:44:41 jsg Exp $

LIB=	LLVMPowerPCAsmParser
NOPROFILE=

CPPFLAGS+=      -I${.OBJDIR}/../include/llvm/PowerPC \
		-I${LLVM_SRCS}/lib/Target/PowerPC

SRCS+=	PPCAsmParser.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/PowerPC/AsmParser
