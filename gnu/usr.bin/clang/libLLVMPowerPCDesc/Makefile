# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:27 patrick Exp $

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
	PPCMachObjectWriter.cpp \
	PPCELFObjectWriter.cpp \
	PPCXCOFFObjectWriter.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/PowerPC/MCTargetDesc
