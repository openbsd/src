# $OpenBSD: Makefile,v 1.3 2019/03/05 09:44:39 jsg Exp $

LIB=	LLVMAArch64AsmParser
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AArch64 \
		-I${LLVM_SRCS}/lib/Target/AArch64

SRCS+=	AArch64AsmParser.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/AArch64/AsmParser
