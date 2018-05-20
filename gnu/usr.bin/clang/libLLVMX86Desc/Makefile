# $OpenBSD: Makefile,v 1.4 2018/04/06 14:44:04 patrick Exp $

LIB=	LLVMX86Desc
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/X86 -I${LLVM_SRCS}/lib/Target/X86

.include <bsd.own.mk>
SRCS=	X86AsmBackend.cpp \
	X86MCTargetDesc.cpp \
	X86MCAsmInfo.cpp \
	X86MCCodeEmitter.cpp \
	X86MachObjectWriter.cpp \
	X86ELFObjectWriter.cpp \
	X86WinCOFFObjectWriter.cpp \
	X86WinCOFFStreamer.cpp \
	X86WinCOFFTargetStreamer.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/X86/MCTargetDesc

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
