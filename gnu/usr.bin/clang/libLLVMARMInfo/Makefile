# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:24 patrick Exp $

LIB=	LLVMARMInfo
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/ARM -I${LLVM_SRCS}/lib/Target/ARM

SRCS+=	ARMTargetInfo.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/ARM/TargetInfo
