# $OpenBSD: Makefile,v 1.12 2019/06/23 22:07:39 patrick Exp $

LIB=	LLVMCodeGen
NOPROFILE=

CPPFLAGS+=-I${LLVM_SRC}/include/llvm/CodeGen
CPPFLAGS+=-I${LLVM_SRC}/include/llvm/CodeGen/PBQP

SRCS+=	AggressiveAntiDepBreaker.cpp \
	AllocationOrder.cpp \
	CodeGenAnalysis.cpp \
	AtomicExpandPass.cpp \
	BasicTargetTransformInfo.cpp \
	BranchFolding.cpp \
	BranchRelaxation.cpp \
	BreakFalseDeps.cpp \
	CFIInstrInserter.cpp \
	CalcSpillWeights.cpp \
	CallingConvLower.cpp \
	CodeGen.cpp \
	CodeGenPrepare.cpp \
	CriticalAntiDepBreaker.cpp \
	DFAPacketizer.cpp \
	DeadMachineInstructionElim.cpp \
	DetectDeadLanes.cpp \
	DwarfEHPrepare.cpp \
	EarlyIfConversion.cpp \
	EdgeBundles.cpp \
	ExecutionDomainFix.cpp \
	ExpandISelPseudos.cpp \
	ExpandMemCmp.cpp \
	ExpandPostRAPseudos.cpp \
	ExpandReductions.cpp \
	FEntryInserter.cpp \
	FaultMaps.cpp \
	FuncletLayout.cpp \
	GCMetadata.cpp \
	GCMetadataPrinter.cpp \
	GCRootLowering.cpp \
	GCStrategy.cpp \
	GlobalMerge.cpp \
	IfConversion.cpp \
	ImplicitNullChecks.cpp \
	IndirectBrExpandPass.cpp \
	InlineSpiller.cpp \
	InterferenceCache.cpp \
	InterleavedAccessPass.cpp \
	InterleavedLoadCombinePass.cpp \
	IntrinsicLowering.cpp \
	LLVMTargetMachine.cpp \
	LatencyPriorityQueue.cpp \
	LazyMachineBlockFrequencyInfo.cpp \
	LexicalScopes.cpp \
	LiveDebugValues.cpp \
	LiveDebugVariables.cpp \
	LiveInterval.cpp \
	LiveIntervalUnion.cpp \
	LiveIntervals.cpp \
	LivePhysRegs.cpp \
	LiveRangeCalc.cpp \
	LiveRangeEdit.cpp \
	LiveRangeShrink.cpp \
	LiveRegMatrix.cpp \
	LiveRegUnits.cpp \
	LiveStacks.cpp \
	LiveVariables.cpp \
	LocalStackSlotAllocation.cpp \
	LoopTraversal.cpp \
	CodeGenLowLevelType.cpp \
	LowerEmuTLS.cpp \
	MachineBasicBlock.cpp \
	MachineBlockFrequencyInfo.cpp \
	MachineBlockPlacement.cpp \
	MachineBranchProbabilityInfo.cpp \
	MachineCSE.cpp \
	MachineCombiner.cpp \
	MachineCopyPropagation.cpp \
	MachineDominators.cpp \
	MachineDominanceFrontier.cpp \
	MachineFrameInfo.cpp \
	MachineFunction.cpp \
	MachineFunctionPass.cpp \
	MachineFunctionPrinterPass.cpp \
	MachineInstr.cpp \
	MachineInstrBundle.cpp \
	MachineLICM.cpp \
	MachineLoopInfo.cpp \
	MachineModuleInfo.cpp \
	MachineModuleInfoImpls.cpp \
	MachineOperand.cpp \
	MachineOptimizationRemarkEmitter.cpp \
	MachineOutliner.cpp \
	MachinePipeliner.cpp \
	MachinePostDominators.cpp \
	MachineRegisterInfo.cpp \
	MachineRegionInfo.cpp \
	MachineSSAUpdater.cpp \
	MachineScheduler.cpp \
	MachineSink.cpp \
	MachineTraceMetrics.cpp \
	MachineVerifier.cpp \
	MacroFusion.cpp \
	MIRCanonicalizerPass.cpp \
	MIRPrinter.cpp \
	MIRPrintingPass.cpp \
	OptimizePHIs.cpp \
	PHIElimination.cpp \
	PHIEliminationUtils.cpp \
	ParallelCG.cpp \
	PatchableFunction.cpp \
	PeepholeOptimizer.cpp \
	PostRAHazardRecognizer.cpp \
	PostRASchedulerList.cpp \
	PreISelIntrinsicLowering.cpp \
	ProcessImplicitDefs.cpp \
	PrologEpilogInserter.cpp \
	PseudoSourceValue.cpp \
	ReachingDefAnalysis.cpp \
	RegAllocBase.cpp \
	RegAllocBasic.cpp \
	RegAllocFast.cpp \
	RegAllocGreedy.cpp \
	RegAllocPBQP.cpp \
	RegUsageInfoCollector.cpp \
	RegUsageInfoPropagate.cpp \
	RegisterClassInfo.cpp \
	RegisterCoalescer.cpp \
	RegisterPressure.cpp \
	RegisterScavenging.cpp \
	RegisterUsageInfo.cpp \
	RenameIndependentSubregs.cpp \
	ResetMachineFunctionPass.cpp \
	ReturnProtectorLowering.cpp \
	ReturnProtectorPass.cpp \
	SafeStack.cpp \
	SafeStackColoring.cpp \
	SafeStackLayout.cpp \
	ScalarizeMaskedMemIntrin.cpp \
	ScheduleDAG.cpp \
	ScheduleDAGInstrs.cpp \
	ScheduleDAGPrinter.cpp \
	ScoreboardHazardRecognizer.cpp \
	ShrinkWrap.cpp \
	ShadowStackGCLowering.cpp \
	SjLjEHPrepare.cpp \
	SlotIndexes.cpp \
	SpillPlacement.cpp \
	SplitKit.cpp \
	StackColoring.cpp \
	StackProtector.cpp \
	StackSlotColoring.cpp \
	StackMapLivenessAnalysis.cpp \
	StackMaps.cpp \
	TailDuplication.cpp \
	TailDuplicator.cpp \
	TargetFrameLoweringImpl.cpp \
	TargetInstrInfo.cpp \
	TargetLoweringBase.cpp \
	TargetLoweringObjectFileImpl.cpp \
	TargetOptionsImpl.cpp \
	TargetPassConfig.cpp \
	TargetRegisterInfo.cpp \
	TargetSchedule.cpp \
	TargetSubtargetInfo.cpp \
	TwoAddressInstructionPass.cpp \
	UnreachableBlockElim.cpp \
	ValueTypes.cpp \
	VirtRegMap.cpp \
	WasmEHPrepare.cpp \
	WinEHPrepare.cpp \
	XRayInstrumentation.cpp

CodeGenAnalysis.cpp:
	ln -s ${LLVM_SRCS}/lib/CodeGen/Analysis.cpp $@

CodeGenLowLevelType.cpp:
	ln -s ${LLVM_SRCS}/lib/CodeGen/LowLevelType.cpp $@

.PATH:	${.CURDIR}/../../../llvm/lib/CodeGen

CLEANFILES+= CodeGenAnalysis.cpp CodeGenLowLevelType.cpp
