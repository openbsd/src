# $OpenBSD: Makefile,v 1.3 2019/03/05 09:44:41 jsg Exp $

LIB=	LLVMMipsInfo
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Mips -I${LLVM_SRCS}/lib/Target/Mips

SRCS+=	MipsTargetInfo.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/Mips/TargetInfo
