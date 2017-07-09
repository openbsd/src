# $OpenBSD: Makefile,v 1.3 2017/07/09 15:28:35 espie Exp $

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
	X86WinCOFFStreamer.cpp \
	X86WinCOFFObjectWriter.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/X86/MCTargetDesc

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
