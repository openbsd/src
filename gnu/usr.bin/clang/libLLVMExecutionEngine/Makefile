# $OpenBSD: Makefile,v 1.6 2021/04/28 12:55:39 patrick Exp $

LIB=	LLVMExecutionEngine
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/ExecutionEngine \
	-I${LLVM_SRCS}/include/llvm/ExecutionEngine/JITLink \
	-I${LLVM_SRCS}/include/llvm/ExecutionEngine/Orc

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
SRCS+=	JITLink.cpp \
	JITLinkGeneric.cpp \
	JITLinkMemoryManager.cpp \
	EHFrameSupport.cpp \
	ExecutionEngineMachO.cpp \
	MachO_arm64.cpp \
	MachO_x86_64.cpp \
	MachOLinkGraphBuilder.cpp \
	ExecutionEngineELF.cpp \
	ELF_x86_64.cpp

# MCJIT
SRCS+=	MCJIT.cpp

# OrcError
SRCS+=	OrcError.cpp \
	RPCError.cpp \

# OrcJIT
SRCS+=	CompileOnDemandLayer.cpp \
	CompileUtils.cpp \
	ExecutionEngineCore.cpp \
	DebugUtils.cpp \
	ExecutionUtils.cpp \
	IndirectionUtils.cpp \
	IRCompileLayer.cpp \
	IRTransformLayer.cpp \
	JITTargetMachineBuilder.cpp \
	LazyReexports.cpp \
	Legacy.cpp \
	Layer.cpp \
	LLJIT.cpp \
	MachOPlatform.cpp \
	Mangling.cpp \
	NullResolver.cpp \
	ObjectLinkingLayer.cpp \
	ObjectTransformLayer.cpp \
	OrcABISupport.cpp \
	OrcCBindings.cpp \
	OrcV2CBindings.cpp \
	OrcMCJITReplacement.cpp \
	RTDyldObjectLinkingLayer.cpp \
	ThreadSafeModule.cpp \
	Speculation.cpp \
	SpeculateAnalyses.cpp

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
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine/OrcError
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine/RuntimeDyld
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine/RuntimeDyld/Targets

CLEANFILES+= ExecutionEngineCore.cpp \
	ExecutionEngineMachO.cpp \
	ExecutionEngineELF.cpp
