# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:49 pascal Exp $

LIB=	LLVMSparcInfo
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Sparc \
		-I${LLVM_SRCS}/lib/Target/Sparc

.include <bsd.own.mk>
SRCS=	SparcTargetInfo.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/Sparc/TargetInfo

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
