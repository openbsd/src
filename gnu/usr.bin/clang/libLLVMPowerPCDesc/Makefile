# $OpenBSD: Makefile,v 1.6 2021/12/17 14:55:45 patrick Exp $

LIB=	LLVMPowerPCDesc
NOPROFILE=

CPPFLAGS+=      -I${.OBJDIR}/../include/llvm/PowerPC \
		-I${LLVM_SRCS}/lib/Target/PowerPC

SRCS+=	PPCAsmBackend.cpp \
	PPCInstPrinter.cpp \
	PPCMCTargetDesc.cpp \
	PPCMCAsmInfo.cpp \
	PPCMCCodeEmitter.cpp \
	PPCMCExpr.cpp \
	PPCPredicates.cpp \
	PPCELFObjectWriter.cpp \
	PPCXCOFFObjectWriter.cpp \
	PPCELFStreamer.cpp \
	PPCXCOFFStreamer.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/PowerPC/MCTargetDesc
