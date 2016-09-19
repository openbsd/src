# $OpenBSD: Makefile,v 1.1 2016/09/19 11:39:05 pascal Exp $

LIB=	LLVMPowerPCDesc
NOPIC=
NOPROFILE=

CPPFLAGS+=      -I${.OBJDIR}/../include/llvm/PowerPC \
		-I${LLVM_SRCS}/lib/Target/PowerPC

.include <bsd.own.mk>
SRCS=	PPCAsmBackend.cpp \
	PPCMCTargetDesc.cpp \
	PPCMCAsmInfo.cpp \
	PPCMCCodeEmitter.cpp \
	PPCMCExpr.cpp \
	PPCPredicates.cpp \
	PPCMachObjectWriter.cpp \
	PPCELFObjectWriter.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/PowerPC/MCTargetDesc

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
