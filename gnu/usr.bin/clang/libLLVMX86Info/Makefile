# $OpenBSD: Makefile,v 1.3 2019/03/05 09:44:42 jsg Exp $

LIB=	LLVMX86Info
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/X86 -I${LLVM_SRCS}/lib/Target/X86

SRCS+=	X86TargetInfo.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/X86/TargetInfo
