# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:49 pascal Exp $

LIB=	LLVMSparcAsmPrinter
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Sparc \
		-I${LLVM_SRCS}/lib/Target/Sparc

.include <bsd.own.mk>
SRCS=	SparcInstPrinter.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/Sparc/InstPrinter

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
