# $OpenBSD: Makefile,v 1.7 2026/05/29 11:18:31 robert Exp $

LIB=	LLVMPowerPCDesc
NOPROFILE=

CPPFLAGS+=      -I${.OBJDIR}/../include/llvm/PowerPC \
		-I${LLVM_SRCS}/lib/Target/PowerPC

SRCS+=	PPCAsmBackend.cpp \
	PPCInstPrinter.cpp \
	PPCMCTargetDesc.cpp \
	PPCMCAsmInfo.cpp \
	PPCMCCodeEmitter.cpp \
	PPCPredicates.cpp \
	PPCELFObjectWriter.cpp \
	PPCXCOFFObjectWriter.cpp \
	PPCELFStreamer.cpp \
	PPCXCOFFStreamer.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/PowerPC/MCTargetDesc
