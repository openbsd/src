# $OpenBSD: Makefile,v 1.3 2019/01/27 16:59:42 patrick Exp $

LIB=	LLVMX86AsmPrinter
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/X86 -I${LLVM_SRCS}/lib/Target/X86

.include <bsd.own.mk>
SRCS=	X86ATTInstPrinter.cpp \
	X86InstPrinterCommon.cpp \
	X86IntelInstPrinter.cpp \
	X86InstComments.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/X86/InstPrinter

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
