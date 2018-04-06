# $OpenBSD: Makefile,v 1.1 2018/04/06 14:44:04 patrick Exp $

LIB=	LLVMARMUtils
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/ARM -I${LLVM_SRCS}/lib/Target/ARM

.include <bsd.own.mk>
SRCS=	ARMBaseInfo.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/ARM/Utils

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
