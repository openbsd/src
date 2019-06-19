# $OpenBSD: Makefile,v 1.6 2019/03/05 09:44:41 jsg Exp $

LIB=	LLVMPowerPCCodeGen
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/PowerPC \
		-I${LLVM_SRCS}/lib/Target/PowerPC

SRCS+=	PPCBoolRetToInt.cpp \
	PPCAsmPrinter.cpp \
	PPCBranchCoalescing.cpp \
	PPCBranchSelector.cpp \
	PPCCCState.cpp \
	PPCCTRLoops.cpp \
	PPCExpandISEL.cpp \
	PPCHazardRecognizers.cpp \
	PPCInstrInfo.cpp \
	PPCISelDAGToDAG.cpp \
	PPCISelLowering.cpp \
	PPCEarlyReturn.cpp \
	PPCFastISel.cpp \
	PPCFrameLowering.cpp \
	PPCLoopPreIncPrep.cpp \
	PPCMCInstLower.cpp \
	PPCMachineFunctionInfo.cpp \
	PPCMIPeephole.cpp \
	PPCPreEmitPeephole.cpp \
	PPCQPXLoadSplat.cpp \
	PPCReduceCRLogicals.cpp \
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
