# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:50 pascal Exp $

LIB=	LLVMX86AsmParser
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/X86 -I${LLVM_SRCS}/lib/Target/X86

.include <bsd.own.mk>
SRCS=	X86AsmInstrumentation.cpp \
	X86AsmParser.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/X86/AsmParser

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
