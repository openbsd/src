# $OpenBSD: Makefile,v 1.11 2023/11/11 18:35:36 robert Exp $

LIB=	LLVMARMCodeGen
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/ARM -I${LLVM_SRCS}/lib/Target/ARM

SRCS+=	A15SDOptimizer.cpp \
	ARMAsmPrinter.cpp \
	ARMBaseInstrInfo.cpp \
	ARMBaseRegisterInfo.cpp \
	ARMBasicBlockInfo.cpp \
	ARMBranchTargets.cpp \
	ARMCallingConv.cpp \
	ARMCallLowering.cpp \
	ARMConstantIslandPass.cpp \
	ARMConstantPoolValue.cpp \
	ARMExpandPseudoInsts.cpp \
	ARMFastISel.cpp \
	ARMFixCortexA57AES1742098Pass.cpp \
	ARMFrameLowering.cpp \
	ARMHazardRecognizer.cpp \
	ARMInstructionSelector.cpp \
	ARMISelDAGToDAG.cpp \
	ARMISelLowering.cpp \
	ARMInstrInfo.cpp \
	ARMLegalizerInfo.cpp \
	ARMParallelDSP.cpp \
	ARMLoadStoreOptimizer.cpp \
	ARMLowOverheadLoops.cpp \
	ARMBlockPlacement.cpp \
	ARMMCInstLower.cpp \
	ARMMachineFunctionInfo.cpp \
	ARMMacroFusion.cpp \
	ARMRegisterInfo.cpp \
	ARMOptimizeBarriersPass.cpp \
	ARMRegisterBankInfo.cpp \
	ARMSelectionDAGInfo.cpp \
	ARMSLSHardening.cpp \
	ARMSubtarget.cpp \
	ARMTargetMachine.cpp \
	ARMTargetObjectFile.cpp \
	ARMTargetTransformInfo.cpp \
	MLxExpansionPass.cpp \
	MVEGatherScatterLowering.cpp \
	MVELaneInterleavingPass.cpp \
	MVETailPredication.cpp \
	MVEVPTBlockPass.cpp \
	MVETPAndVPTOptimisationsPass.cpp \
	Thumb1FrameLowering.cpp \
	Thumb1InstrInfo.cpp \
	ThumbRegisterInfo.cpp \
	Thumb2ITBlockPass.cpp \
	Thumb2InstrInfo.cpp \
	Thumb2SizeReduction.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/ARM
