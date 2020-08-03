# $OpenBSD: Makefile,v 1.3 2020/08/03 14:45:24 patrick Exp $

LIB=	LLVMARMUtils
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/ARM -I${LLVM_SRCS}/lib/Target/ARM

SRCS+=	ARMBaseInfo.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/ARM/Utils
