# $OpenBSD: Makefile,v 1.2 2017/01/14 20:12:41 patrick Exp $

LIB=	LLVMCodeGen
NOPIC=
NOPROFILE=

CPPFLAGS+=-I${LLVM_SRC}/include/llvm/CodeGen
CPPFLAGS+=-I${LLVM_SRC}/include/llvm/CodeGen/PBQP

.include <bsd.own.mk>
SRCS=	AggressiveAntiDepBreaker.cpp \
	AllocationOrder.cpp \
	Analysis.cpp \
	AtomicExpandPass.cpp \
	BasicTargetTransformInfo.cpp \
	BranchFolding.cpp \
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
	ExecutionDepsFix.cpp \
	ExpandISelPseudos.cpp \
	ExpandPostRAPseudos.cpp \
	LiveDebugValues.cpp \
	FaultMaps.cpp \
	FuncletLayout.cpp \
	GCMetadata.cpp \
	GCMetadataPrinter.cpp \
	GCRootLowering.cpp \
	GCStrategy.cpp \
	GlobalMerge.cpp \
	IfConversion.cpp \
	ImplicitNullChecks.cpp \
	InlineSpiller.cpp \
	InterferenceCache.cpp \
	InterleavedAccessPass.cpp \
	IntrinsicLowering.cpp \
	LLVMTargetMachine.cpp \
	LatencyPriorityQueue.cpp \
	LexicalScopes.cpp \
	LiveDebugVariables.cpp \
	LiveInterval.cpp \
	LiveIntervalAnalysis.cpp \
	LiveIntervalUnion.cpp \
	LiveRangeCalc.cpp \
	LiveRangeEdit.cpp \
	LiveRegMatrix.cpp \
	LivePhysRegs.cpp \
	LiveStackAnalysis.cpp \
	LiveVariables.cpp \
	LocalStackSlotAllocation.cpp \
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
	MachineFunction.cpp \
	MachineFunctionAnalysis.cpp \
	MachineFunctionPass.cpp \
	MachineFunctionPrinterPass.cpp \
	MachineInstr.cpp \
	MachineInstrBundle.cpp \
	MachineLICM.cpp \
	MachineLoopInfo.cpp \
	MachineModuleInfo.cpp \
	MachineModuleInfoImpls.cpp \
	MachinePassRegistry.cpp \
	MachinePostDominators.cpp \
	MachineRegisterInfo.cpp \
	MachineRegionInfo.cpp \
	MachineSSAUpdater.cpp \
	MachineScheduler.cpp \
	MachineSink.cpp \
	MachineTraceMetrics.cpp \
	MachineVerifier.cpp \
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
	SafeStack.cpp \
	SafeStackColoring.cpp \
	SafeStackLayout.cpp \
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
	TwoAddressInstructionPass.cpp \
	UnreachableBlockElim.cpp \
	VirtRegMap.cpp \
	WinEHPrepare.cpp \
	XRayInstrumentation.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/CodeGen

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
