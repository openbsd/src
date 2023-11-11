# $OpenBSD: Makefile,v 1.5 2023/11/11 18:35:36 robert Exp $

LIB=	LLVMAArch64Utils
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AArch64 \
		-I${LLVM_SRCS}/lib/Target/AArch64

SRCS+=	AArch64BaseInfo.cpp \
	AArch64SMEAttributes.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/AArch64/Utils
