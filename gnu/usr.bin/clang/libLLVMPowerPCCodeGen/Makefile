# $OpenBSD: Makefile,v 1.9 2021/04/28 12:55:40 patrick Exp $

LIB=	LLVMPowerPCCodeGen
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/PowerPC \
		-I${LLVM_SRCS}/lib/Target/PowerPC

SRCS+=	PPCBoolRetToInt.cpp \
	PPCAsmPrinter.cpp \
	PPCBranchSelector.cpp \
	PPCBranchCoalescing.cpp \
	PPCCallingConv.cpp \
	PPCCCState.cpp \
	PPCCTRLoops.cpp \
	PPCHazardRecognizers.cpp \
	PPCInstrInfo.cpp \
	PPCISelDAGToDAG.cpp \
	PPCISelLowering.cpp \
	PPCEarlyReturn.cpp \
	PPCFastISel.cpp \
	PPCFrameLowering.cpp \
	PPCLoopInstrFormPrep.cpp \
	PPCMCInstLower.cpp \
	PPCMachineFunctionInfo.cpp \
	PPCMachineScheduler.cpp \
	PPCMacroFusion.cpp \
	PPCMIPeephole.cpp \
	PPCRegisterInfo.cpp \
	PPCReturnProtectorLowering.cpp \
	PPCQPXLoadSplat.cpp \
	PPCSubtarget.cpp \
	PPCTargetMachine.cpp \
	PPCTargetObjectFile.cpp \
	PPCTargetTransformInfo.cpp \
	PPCTOCRegDeps.cpp \
	PPCTLSDynamicCall.cpp \
	PPCVSXCopy.cpp \
	PPCReduceCRLogicals.cpp \
	PPCVSXFMAMutate.cpp \
	PPCVSXSwapRemoval.cpp \
	PPCExpandISEL.cpp \
	PPCPreEmitPeephole.cpp \
	PPCLowerMASSVEntries.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/PowerPC
