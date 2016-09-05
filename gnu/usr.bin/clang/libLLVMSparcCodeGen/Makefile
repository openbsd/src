# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:49 pascal Exp $

LIB=	LLVMSparcCodeGen
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Sparc \
		-I${LLVM_SRCS}/lib/Target/Sparc

.include <bsd.own.mk>
SRCS=	DelaySlotFiller.cpp \
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

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
