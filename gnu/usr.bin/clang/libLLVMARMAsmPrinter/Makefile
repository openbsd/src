# $OpenBSD: Makefile,v 1.1 2016/09/17 16:43:51 kettenis Exp $

LIB=	LLVMARMAsmPrinter
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/ARM -I${LLVM_SRCS}/lib/Target/ARM

.include <bsd.own.mk>
SRCS=	ARMInstPrinter.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/ARM/InstPrinter

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
