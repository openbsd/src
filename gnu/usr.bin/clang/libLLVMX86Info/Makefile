# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMX86Info
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/X86 -I${LLVM_SRCS}/lib/Target/X86

.include <bsd.own.mk>
SRCS=	X86TargetInfo.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/X86/TargetInfo

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
