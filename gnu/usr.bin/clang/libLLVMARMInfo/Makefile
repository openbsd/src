# $OpenBSD: Makefile,v 1.3 2019/03/05 09:44:39 jsg Exp $

LIB=	LLVMARMInfo
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/ARM -I${LLVM_SRCS}/lib/Target/ARM

SRCS+=	ARMTargetInfo.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/ARM/TargetInfo
