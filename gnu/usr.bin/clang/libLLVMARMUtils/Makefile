# $OpenBSD: Makefile,v 1.2 2019/03/05 09:44:40 jsg Exp $

LIB=	LLVMARMUtils
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/ARM -I${LLVM_SRCS}/lib/Target/ARM

SRCS+=	ARMBaseInfo.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/ARM/Utils
