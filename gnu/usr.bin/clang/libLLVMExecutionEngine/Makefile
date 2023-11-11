# $OpenBSD: Makefile,v 1.8 2023/11/11 18:35:37 robert Exp $

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
SRCS+=	DWARFRecordSectionSplitter.cpp \
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
	ELF_aarch64.cpp \
	ELF_i386.cpp \
	ELF_loongarch.cpp \
	ELF_riscv.cpp \
	ELF_x86_64.cpp \
	ExecutionEngineCOFF.cpp \
	COFFDirectiveParser.cpp \
	COFFLinkGraphBuilder.cpp \
	COFF_x86_64.cpp \
	aarch64.cpp \
	i386.cpp \
	loongarch.cpp \
	riscv.cpp \
	x86_64.cpp

# MCJIT
SRCS+=	MCJIT.cpp

# OrcShared
SRCS+=	AllocationActions.cpp \
	OrcError.cpp \
	OrcRTBridge.cpp \
	SimpleRemoteEPCUtils.cpp

# OrcTargetProcess
SRCS+=	ExecutorSharedMemoryMapperService.cpp \
	JITLoaderGDB.cpp \
	OrcRTBootstrap.cpp \
	RegisterEHFrames.cpp \
	SimpleExecutorDylibManager.cpp \
	SimpleExecutorMemoryManager.cpp \
	SimpleRemoteEPCServer.cpp \
	TargetExecutionUtils.cpp

# Orc
SRCS+=	COFFVCRuntimeSupport.cpp \
	COFFPlatform.cpp \
	CompileOnDemandLayer.cpp \
	CompileUtils.cpp \
	ExecutionEngineCore.cpp \
	DebugObjectManagerPlugin.cpp \
	DebuggerSupportPlugin.cpp \
	DebugUtils.cpp \
	EPCDynamicLibrarySearchGenerator.cpp \
	EPCDebugObjectRegistrar.cpp \
	EPCEHFrameRegistrar.cpp \
	EPCGenericDylibManager.cpp \
	EPCGenericJITLinkMemoryManager.cpp \
	EPCGenericRTDyldMemoryManager.cpp \
	EPCIndirectionUtils.cpp \
	ExecutionUtils.cpp \
	ObjectFileInterface.cpp \
	IndirectionUtils.cpp \
	IRCompileLayer.cpp \
	IRTransformLayer.cpp \
	JITTargetMachineBuilder.cpp \
	LazyReexports.cpp \
	Layer.cpp \
	LookupAndRecordAddrs.cpp \
	LLJIT.cpp \
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
	SimpleRemoteEPC.cpp \
	Speculation.cpp \
	SpeculateAnalyses.cpp \
	ExecutorProcessControl.cpp \
	TaskDispatch.cpp \
	ThreadSafeModule.cpp

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

ExecutionEngineELF.cpp:
	ln -s ${LLVM_SRCS}/lib/ExecutionEngine/JITLink/ELF.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine/Interpreter
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine/JITLink
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine/MCJIT
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine/Orc
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine/Orc/Shared
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine/Orc/TargetProcess
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine/RuntimeDyld
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine/RuntimeDyld/Targets

CLEANFILES+= ExecutionEngineCore.cpp \
	ExecutionEngineMachO.cpp \
	ExecutionEngineELF.cpp
