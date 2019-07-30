# $OpenBSD: Makefile,v 1.2 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMX86AsmParser
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/X86 -I${LLVM_SRCS}/lib/Target/X86

.include <bsd.own.mk>
SRCS=	X86AsmInstrumentation.cpp \
	X86AsmParser.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/X86/AsmParser

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
