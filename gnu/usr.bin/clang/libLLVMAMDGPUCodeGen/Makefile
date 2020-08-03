# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:24 patrick Exp $

LIB=	LLVMAMDGPUCodeGen
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AMDGPU \
		-I${LLVM_SRCS}/lib/Target/AMDGPU

SRCS+=	AMDGPUAliasAnalysis.cpp \
	AMDGPUAlwaysInlinePass.cpp \
	AMDGPUAnnotateKernelFeatures.cpp \
	AMDGPUAnnotateUniformValues.cpp \
	AMDGPUArgumentUsageInfo.cpp \
	AMDGPUAsmPrinter.cpp \
	AMDGPUAtomicOptimizer.cpp \
	AMDGPUCallLowering.cpp \
	AMDGPUCodeGenPrepare.cpp \
	AMDGPUFixFunctionBitcasts.cpp \
	AMDGPUFrameLowering.cpp \
	AMDGPUHSAMetadataStreamer.cpp \
	AMDGPUInstrInfo.cpp \
	AMDGPUInstructionSelector.cpp \
	AMDGPUISelDAGToDAG.cpp \
	AMDGPUISelLowering.cpp \
	AMDGPUGlobalISelUtils.cpp \
	AMDGPULegalizerInfo.cpp \
	AMDGPULibCalls.cpp \
	AMDGPULibFunc.cpp \
	AMDGPULowerIntrinsics.cpp \
	AMDGPULowerKernelArguments.cpp \
	AMDGPULowerKernelAttributes.cpp \
	AMDGPUMachineCFGStructurizer.cpp \
	AMDGPUMachineFunction.cpp \
	AMDGPUMachineModuleInfo.cpp \
	AMDGPUMacroFusion.cpp \
	AMDGPUMCInstLower.cpp \
	AMDGPUOpenCLEnqueuedBlockLowering.cpp \
	AMDGPUPromoteAlloca.cpp \
	AMDGPUPropagateAttributes.cpp \
	AMDGPURegisterBankInfo.cpp \
	AMDGPURegisterInfo.cpp \
	AMDGPURewriteOutArguments.cpp \
	AMDGPUSubtarget.cpp \
	AMDGPUTargetMachine.cpp \
	AMDGPUTargetObjectFile.cpp \
	AMDGPUTargetTransformInfo.cpp \
	AMDGPUUnifyDivergentExitNodes.cpp \
	AMDGPUUnifyMetadata.cpp \
	AMDGPUInline.cpp \
	AMDGPUPerfHintAnalysis.cpp \
	AMDILCFGStructurizer.cpp \
	AMDGPUPrintfRuntimeBinding.cpp \
	GCNDPPCombine.cpp \
	GCNHazardRecognizer.cpp \
	GCNILPSched.cpp \
	GCNRegBankReassign.cpp \
	GCNNSAReassign.cpp \
	GCNIterativeScheduler.cpp \
	GCNMinRegStrategy.cpp \
	GCNRegPressure.cpp \
	GCNSchedStrategy.cpp \
	R600AsmPrinter.cpp \
	R600ClauseMergePass.cpp \
	R600ControlFlowFinalizer.cpp \
	R600EmitClauseMarkers.cpp \
	R600ExpandSpecialInstrs.cpp \
	R600FrameLowering.cpp \
	R600InstrInfo.cpp \
	R600ISelLowering.cpp \
	R600MachineFunctionInfo.cpp \
	R600MachineScheduler.cpp \
	R600OpenCLImageTypeLoweringPass.cpp \
	R600OptimizeVectorRegisters.cpp \
	R600Packetizer.cpp \
	R600RegisterInfo.cpp \
	SIAddIMGInit.cpp \
	SIAnnotateControlFlow.cpp \
	SIFixSGPRCopies.cpp \
	SIFixVGPRCopies.cpp \
	SIPreAllocateWWMRegs.cpp \
	SIFixupVectorISel.cpp \
	SIFoldOperands.cpp \
	SIFormMemoryClauses.cpp \
	SIFrameLowering.cpp \
	SIInsertSkips.cpp \
	SIInsertWaitcnts.cpp \
	SIInstrInfo.cpp \
	SIISelLowering.cpp \
	SILoadStoreOptimizer.cpp \
	SILowerControlFlow.cpp \
	SILowerI1Copies.cpp \
	SILowerSGPRSpills.cpp \
	SIMachineFunctionInfo.cpp \
	SIMachineScheduler.cpp \
	SIMemoryLegalizer.cpp \
	SIModeRegister.cpp \
	SIOptimizeExecMasking.cpp \
	SIOptimizeExecMaskingPreRA.cpp \
	SIPeepholeSDWA.cpp \
	SIRegisterInfo.cpp \
	SIShrinkInstructions.cpp \
	SIWholeQuadMode.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/AMDGPU
