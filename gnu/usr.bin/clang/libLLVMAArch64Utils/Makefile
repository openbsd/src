# $OpenBSD: Makefile,v 1.6 2026/05/29 11:18:30 robert Exp $

LIB=	LLVMAArch64Utils
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AArch64 \
		-I${LLVM_SRCS}/lib/Target/AArch64

SRCS+=	AArch64BaseInfo.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/AArch64/Utils
