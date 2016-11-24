# $OpenBSD: Makefile,v 1.1 2016/11/24 22:32:29 patrick Exp $

LIB=	LLVMAArch64Info
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AArch64 \
		-I${LLVM_SRCS}/lib/Target/AArch64

.include <bsd.own.mk>
SRCS=	AArch64TargetInfo.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/AArch64/TargetInfo

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
