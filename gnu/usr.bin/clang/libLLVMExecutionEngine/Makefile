# $OpenBSD: Makefile,v 1.5 2020/08/03 14:45:25 patrick Exp $

LIB=	LLVMExecutionEngine
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/ExecutionEngine

SRCS+=	CompileUtils.cpp \
	DebugUtils.cpp \
	Execution.cpp \
	ExecutionEngine.cpp \
	EHFrameSupport.cpp \
	ExecutionEngineBindings.cpp \
	ExecutionEngineCore.cpp \
	ExecutionUtils.cpp \
	ExternalFunctions.cpp \
	GDBRegistrationListener.cpp \
	IRCompileLayer.cpp \
	IndirectionUtils.cpp \
	Interpreter.cpp \
	JITLink.cpp \
	JITLinkGeneric.cpp \
	JITLinkMemoryManager.cpp \
	JITSymbol.cpp \
	JITTargetMachineBuilder.cpp \
	Layer.cpp \
	Legacy.cpp \
	MCJIT.cpp \
	MachO.cpp \
	MachOLinkGraphBuilder.cpp \
	MachO_arm64.cpp \
	MachO_x86_64.cpp \
	NullResolver.cpp \
	ObjectLinkingLayer.cpp \
	OrcABISupport.cpp \
	OrcCBindings.cpp \
	OrcError.cpp \
	OrcMCJITReplacement.cpp \
	RPCError.cpp \
	RTDyldMemoryManager.cpp \
	RuntimeDyld.cpp \
	RuntimeDyldCOFF.cpp \
	RuntimeDyldChecker.cpp \
	RuntimeDyldELF.cpp \
	RuntimeDyldELFMips.cpp \
	RuntimeDyldMachO.cpp \
	SectionMemoryManager.cpp \
	SpeculateAnalyses.cpp \
	Speculation.cpp \
	TargetSelect.cpp \
	ThreadSafeModule.cpp

ExecutionEngineCore.cpp:
	ln -s ${LLVM_SRCS}/lib/ExecutionEngine/Orc/Core.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine/Interpreter
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine/JITLink
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine/MCJIT
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine/Orc
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine/OrcError
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine/RuntimeDyld
.PATH:	${.CURDIR}/../../../llvm/llvm/lib/ExecutionEngine/RuntimeDyld/Targets

CLEANFILES+= ExecutionEngineCore.cpp
