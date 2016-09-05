# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:50 pascal Exp $

LIB=	LLVMX86Utils
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/X86 -I${LLVM_SRCS}/lib/Target/X86

.include <bsd.own.mk>
SRCS=	X86ShuffleDecode.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/X86/Utils

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
