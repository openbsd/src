# $OpenBSD: Makefile,v 1.3 2017/07/09 15:28:35 espie Exp $

LIB=	LLVMSparcCodeGen
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Sparc \
		-I${LLVM_SRCS}/lib/Target/Sparc

.include <bsd.own.mk>
SRCS=	DelaySlotFiller.cpp \
	LeonPasses.cpp \
	SparcAsmPrinter.cpp \
	SparcInstrInfo.cpp \
	SparcISelDAGToDAG.cpp \
	SparcISelLowering.cpp \
	SparcFrameLowering.cpp \
	SparcMachineFunctionInfo.cpp \
	SparcRegisterInfo.cpp \
	SparcSubtarget.cpp \
	SparcTargetMachine.cpp \
	SparcMCInstLower.cpp \
	SparcTargetObjectFile.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/Sparc

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
