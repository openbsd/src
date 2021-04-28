# $OpenBSD: Makefile,v 1.5 2021/04/28 12:55:40 patrick Exp $

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
	PPCELFStreamer.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/PowerPC/MCTargetDesc
