# $OpenBSD: Makefile,v 1.4 2019/03/05 09:44:42 jsg Exp $

LIB=	LLVMX86AsmPrinter
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/X86 -I${LLVM_SRCS}/lib/Target/X86

SRCS+=	X86ATTInstPrinter.cpp \
	X86InstPrinterCommon.cpp \
	X86IntelInstPrinter.cpp \
	X86InstComments.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/X86/InstPrinter
