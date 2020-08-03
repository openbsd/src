# $OpenBSD: Makefile,v 1.7 2020/08/03 14:45:27 patrick Exp $

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
	PPCCallingConv.cpp \
	PPCExpandISEL.cpp \
	PPCHazardRecognizers.cpp \
	PPCInstrInfo.cpp \
	PPCISelDAGToDAG.cpp \
	PPCISelLowering.cpp \
	PPCEarlyReturn.cpp \
	PPCFastISel.cpp \
	PPCFrameLowering.cpp \
	PPCLoopInstrFormPrep.cpp \
	PPCLowerMASSVEntries.cpp \
	PPCMCInstLower.cpp \
	PPCMachineFunctionInfo.cpp \
	PPCMachineScheduler.cpp \
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

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/PowerPC
