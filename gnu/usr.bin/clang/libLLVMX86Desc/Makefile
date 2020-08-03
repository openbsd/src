# $OpenBSD: Makefile,v 1.6 2020/08/03 14:45:28 patrick Exp $

LIB=	LLVMX86Desc
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/X86 -I${LLVM_SRCS}/lib/Target/X86

SRCS+=	X86ATTInstPrinter.cpp \
	X86IntelInstPrinter.cpp \
	X86InstComments.cpp \
	X86InstPrinterCommon.cpp \
	X86AsmBackend.cpp \
	X86MCTargetDesc.cpp \
	X86MCAsmInfo.cpp \
	X86MCCodeEmitter.cpp \
	X86MachObjectWriter.cpp \
	X86ELFObjectWriter.cpp \
	X86WinCOFFObjectWriter.cpp \
	X86WinCOFFStreamer.cpp \
	X86WinCOFFTargetStreamer.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/X86/MCTargetDesc
