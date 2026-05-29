# $OpenBSD: Makefile,v 1.11 2026/05/29 11:18:30 robert Exp $

LIB=	LLVMExecutionEngine
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/ExecutionEngine \
		-I${LLVM_SRCS}/include/llvm/ExecutionEngine/JITLink \
		-I${LLVM_SRCS}/include/llvm/ExecutionEngine/Orc \
		-I${.OBJDIR}/../include/llvm/ExecutionEngine/JITLink

# ExecutionEngine
SRCS+=	ExecutionEngine.cpp \
	ExecutionEngineBindings.cpp \
	GDBRegistrationListener.cpp \
	SectionMemoryManager.cpp \
	TargetSelect.cpp

# Interpreter
SRCS+=	Execution.cpp \
	ExternalFunctions.cpp \
	Interpreter.cpp

# JITLink
SRCS+=	CompactUnwindSupport.cpp \
	DWARFRecordSectionSplitter.cpp \
	EHFrameSupport.cpp \
	JITLink.cpp \
	JITLinkGeneric.cpp \
	JITLinkMemoryManager.cpp \
	ExecutionEngineMachO.cpp \
	MachO_arm64.cpp \
	MachO_x86_64.cpp \
	MachOLinkGraphBuilder.cpp \
	ExecutionEngineELF.cpp \
	ELFLinkGraphBuilder.cpp \
	ELF_aarch32.cpp \
	ELF_aarch64.cpp \
	ELF_loongarch.cpp \
	ELF_ppc64.cpp \
	ELF_riscv.cpp \
	ELF_systemz.cpp \
	ELF_x86.cpp \
	ELF_x86_64.cpp \
	ExecutionEngineCOFF.cpp \
	COFFDirectiveParser.cpp \
	COFFLinkGraphBuilder.cpp \
	COFF_x86_64.cpp \
	ExecutionEngineJITLinkXCOFF.cpp \
	XCOFF_ppc64.cpp \
	XCOFFLinkGraphBuilder.cpp \
	aarch32.cpp \
	aarch64.cpp \
	loongarch.cpp \
	ppc64.cpp \
	riscv.cpp \
	systemz.cpp \
	x86.cpp \
	x86_64.cpp

# MCJIT
SRCS+=	MCJIT.cpp

# Debugging
SRCS+=	DebugInfoSupport.cpp \
	DebuggerSupport.cpp \
	DebuggerSupportPlugin.cpp \
	ELFDebugObjectPlugin.cpp \
	LLJITUtilsCBindings.cpp \
	PerfSupportPlugin.cpp \
	VTuneSupportPlugin.cpp

# OrcShared
SRCS+=	AllocationActions.cpp \
	MachOObjectFormat.cpp \
	ObjectFormats.cpp \
	OrcError.cpp \
	OrcRTBridge.cpp \
	SimpleRemoteEPCUtils.cpp \
	SymbolStringPool.cpp

# OrcTargetProcess
SRCS+=	ExecutorSharedMemoryMapperService.cpp \
	DefaultHostBootstrapValues.cpp \
	ExecutorResolver.cpp \
	LibraryResolver.cpp \
	JITLoaderGDB.cpp \
	JITLoaderPerf.cpp \
	JITLoaderVTune.cpp \
	LibraryScanner.cpp \
	OrcRTBootstrap.cpp \
	RegisterEHFrames.cpp \
	SimpleExecutorDylibManager.cpp \
	SimpleExecutorMemoryManager.cpp \
	SimpleRemoteEPCServer.cpp \
	TargetExecutionUtils.cpp \
	UnwindInfoManager.cpp

# Orc
SRCS+=	AbsoluteSymbols.cpp \
	BacktraceTools.cpp \
	ExecutionEngineOrcCOFF.cpp \
	COFFVCRuntimeSupport.cpp \
	COFFPlatform.cpp \
	CompileOnDemandLayer.cpp \
	CompileUtils.cpp \
	ExecutionEngineCore.cpp \
	DebugUtils.cpp \
	EHFrameRegistrationPlugin.cpp \
	EPCDynamicLibrarySearchGenerator.cpp \
	EPCGenericDylibManager.cpp \
	EPCGenericJITLinkMemoryManager.cpp \
	EPCGenericRTDyldMemoryManager.cpp \
	EPCIndirectionUtils.cpp \
	ExecutionUtils.cpp \
	ExecutorResolutionGenerator.cpp \
	ObjectFileInterface.cpp \
	IndirectionUtils.cpp \
	InProcessMemoryAccess.cpp \
	IRCompileLayer.cpp \
	IRTransformLayer.cpp \
	IRPartitionLayer.cpp \
	JITTargetMachineBuilder.cpp \
	JITLinkReentryTrampolines.cpp \
	LazyObjectLinkingLayer.cpp \
	LazyReexports.cpp \
	Layer.cpp \
	LinkGraphLayer.cpp \
	LinkGraphLinkingLayer.cpp \
	LoadLinkableFile.cpp \
	LookupAndRecordAddrs.cpp \
	LLJIT.cpp \
	ExecutionEngineOrcMachO.cpp \
	MachOPlatform.cpp \
	MapperJITLinkMemoryManager.cpp \
	MemoryMapper.cpp \
	ELFNixPlatform.cpp \
	Mangling.cpp \
	ObjectLinkingLayer.cpp \
	ObjectTransformLayer.cpp \
	OrcABISupport.cpp \
	OrcV2CBindings.cpp \
	RTDyldObjectLinkingLayer.cpp \
	SectCreate.cpp \
	SelfExecutorProcessControl.cpp \
	SimpleRemoteEPC.cpp \
	SimpleRemoteMemoryMapper.cpp \
	Speculation.cpp \
	SpeculateAnalyses.cpp \
	ExecutorProcessControl.cpp \
	TaskDispatch.cpp \
	ThreadSafeModule.cpp \
	UnwindInfoRegistrationPlugin.cpp \
	RedirectionManager.cpp \
	JITLinkRedirectableSymbolManager.cpp \
	ReOptimizeLayer.cpp

# RuntimeDyld
SRCS+=	JITSymbol.cpp \
	RTDyldMemoryManager.cpp \
	RuntimeDyld.cpp \
	RuntimeDyldChecker.cpp \
	RuntimeDyldCOFF.cpp \
	RuntimeDyldELF.cpp \
	RuntimeDyldMachO.cpp

# RuntimeDyld/Targets
SRCS+=	RuntimeDyldELFMips.cpp

ExecutionEngineCOFF.cpp:
	ln -s ${LLVM_SRCS}/lib/ExecutionEngine/JITLink/COFF.cpp $@

ExecutionEngineCore.cpp:
	ln -s ${LLVM_SRCS}/lib/ExecutionEngine/Orc/Core.cpp $@

ExecutionEngineMachO.cpp:
	ln -s ${LLVM_SRCS}/lib/ExecutionEngine/JITLink/MachO.cpp $@

ExecutionEngineOrcCOFF.cpp:
	ln -s ${LLVM_SRCS}/lib/ExecutionEngine/Orc/COFF.cpp $@

ExecutionEngineOrcMachO.cpp:
	ln -s ${LLVM_SRCS}/lib/ExecutionEngine/Orc/MachO.cpp $@

ExecutionEngineELF.cpp:
	ln -s ${LLVM_SRCS}/lib/ExecutionEngine/JITLink/ELF.cpp $@

ExecutionEngineJITLinkXCOFF.cpp:
	ln -s ${LLVM_SRCS}/lib/ExecutionEngine/JITLink/XCOFF.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine/Interpreter
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine/JITLink
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine/MCJIT
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine/Orc
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine/Orc/Debugging
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine/Orc/Shared
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine/Orc/TargetProcess
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine/RuntimeDyld
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine/RuntimeDyld/Targets

CLEANFILES+= ExecutionEngineCOFF.cpp \
	ExecutionEngineCore.cpp \
	ExecutionEngineMachO.cpp \
	ExecutionEngineOrcCOFF.cpp \
	ExecutionEngineOrcMachO.cpp \
	ExecutionEngineELF.cpp \
	ExecutionEngineJITLinkXCOFF.cpp
