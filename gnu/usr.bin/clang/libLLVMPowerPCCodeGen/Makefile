# $OpenBSD: Makefile,v 1.1 2016/09/19 11:39:05 pascal Exp $

LIB=	LLVMPowerPCCodeGen
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/PowerPC \
		-I${LLVM_SRCS}/lib/Target/PowerPC

.include <bsd.own.mk>
SRCS=	PPCBoolRetToInt.cpp \
	PPCAsmPrinter.cpp \
	PPCBranchSelector.cpp \
	PPCCTRLoops.cpp \
	PPCHazardRecognizers.cpp \
	PPCInstrInfo.cpp \
	PPCISelDAGToDAG.cpp \
	PPCISelLowering.cpp \
	PPCEarlyReturn.cpp \
	PPCFastISel.cpp \
	PPCFrameLowering.cpp \
	PPCLoopDataPrefetch.cpp \
	PPCLoopPreIncPrep.cpp \
	PPCMCInstLower.cpp \
	PPCMachineFunctionInfo.cpp \
	PPCMIPeephole.cpp \
	PPCRegisterInfo.cpp \
	PPCSubtarget.cpp \
	PPCTargetMachine.cpp \
	PPCTargetObjectFile.cpp \
	PPCTargetTransformInfo.cpp \
	PPCTOCRegDeps.cpp \
	PPCTLSDynamicCall.cpp \
	PPCVSXCopy.cpp \
	PPCVSXFMAMutate.cpp \
	PPCVSXSwapRemoval.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/PowerPC

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
