# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:50 pascal Exp $

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
	X86WinCOFFObjectWriter.cpp \
	X86MachORelocationInfo.cpp \
	X86ELFRelocationInfo.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/X86/MCTargetDesc

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
