# $OpenBSD: Makefile,v 1.1 2016/09/17 16:43:51 kettenis Exp $

LIB=	LLVMARMAsmParser
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/ARM -I${LLVM_SRCS}/lib/Target/ARM

.include <bsd.own.mk>
SRCS=	ARMAsmParser.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/ARM/AsmParser

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
