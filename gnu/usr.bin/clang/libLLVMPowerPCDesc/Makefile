# $OpenBSD: Makefile,v 1.3 2019/03/05 09:44:41 jsg Exp $

LIB=	LLVMPowerPCDesc
NOPROFILE=

CPPFLAGS+=      -I${.OBJDIR}/../include/llvm/PowerPC \
		-I${LLVM_SRCS}/lib/Target/PowerPC

SRCS+=	PPCAsmBackend.cpp \
	PPCMCTargetDesc.cpp \
	PPCMCAsmInfo.cpp \
	PPCMCCodeEmitter.cpp \
	PPCMCExpr.cpp \
	PPCPredicates.cpp \
	PPCMachObjectWriter.cpp \
	PPCELFObjectWriter.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/PowerPC/MCTargetDesc
