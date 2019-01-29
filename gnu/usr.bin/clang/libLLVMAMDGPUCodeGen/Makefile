# $OpenBSD: Makefile,v 1.1 2019/01/29 05:51:30 jsg Exp $

LIB=	LLVMAMDGPUCodeGen
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AMDGPU \
		-I${LLVM_SRCS}/lib/Target/AMDGPU

.include <bsd.own.mk>
SRCS=	AMDGPUAliasAnalysis.cpp \
	AMDGPUAlwaysInlinePass.cpp \
	AMDGPUAnnotateKernelFeatures.cpp \
	AMDGPUAnnotateUniformValues.cpp \
	AMDGPUArgumentUsageInfo.cpp \
	AMDGPUAsmPrinter.cpp \
	AMDGPUCallLowering.cpp \
	AMDGPUCodeGenPrepare.cpp \
	AMDGPUFrameLowering.cpp \
	AMDGPUHSAMetadataStreamer.cpp \
	AMDGPUInstrInfo.cpp \
	AMDGPUInstructionSelector.cpp \
	AMDGPUIntrinsicInfo.cpp \
	AMDGPUISelDAGToDAG.cpp \
	AMDGPUISelLowering.cpp \
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
	AMDGPURegAsmNames.inc.cpp \
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
	GCNHazardRecognizer.cpp \
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
	SIAnnotateControlFlow.cpp \
	SIDebuggerInsertNops.cpp \
	SIFixSGPRCopies.cpp \
	SIFixVGPRCopies.cpp \
	SIFixWWMLiveness.cpp \
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
	SIMachineFunctionInfo.cpp \
	SIMachineScheduler.cpp \
	SIMemoryLegalizer.cpp \
	SIOptimizeExecMasking.cpp \
	SIOptimizeExecMaskingPreRA.cpp \
	SIPeepholeSDWA.cpp \
	SIRegisterInfo.cpp \
	SIShrinkInstructions.cpp \
	SIWholeQuadMode.cpp \
	GCNILPSched.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/AMDGPU

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
