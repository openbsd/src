# $OpenBSD: Makefile,v 1.1 2017/01/25 19:24:33 patrick Exp $

LIB=	LLVMMipsInfo
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Mips -I${LLVM_SRCS}/lib/Target/Mips

.include <bsd.own.mk>
SRCS=	MipsTargetInfo.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/Mips/TargetInfo

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
