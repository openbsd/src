# $OpenBSD: Makefile,v 1.5 2019/03/05 09:44:42 jsg Exp $

LIB=	LLVMX86Desc
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/X86 -I${LLVM_SRCS}/lib/Target/X86

SRCS+=	X86AsmBackend.cpp \
	X86MCTargetDesc.cpp \
	X86MCAsmInfo.cpp \
	X86MCCodeEmitter.cpp \
	X86MachObjectWriter.cpp \
	X86ELFObjectWriter.cpp \
	X86WinCOFFObjectWriter.cpp \
	X86WinCOFFStreamer.cpp \
	X86WinCOFFTargetStreamer.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/X86/MCTargetDesc
