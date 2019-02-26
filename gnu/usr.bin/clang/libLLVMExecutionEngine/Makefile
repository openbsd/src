# $OpenBSD: Makefile,v 1.3 2019/02/26 23:39:51 jsg Exp $

LIB=	LLVMExecutionEngine
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/ExecutionEngine

.include <bsd.own.mk>
SRCS=	Execution.cpp \
	ExecutionEngine.cpp \
	ExecutionEngineCore.cpp \
	ExecutionEngineBindings.cpp \
	ExecutionUtils.cpp \
	ExternalFunctions.cpp \
	GDBRegistrationListener.cpp \
	IndirectionUtils.cpp \
	Interpreter.cpp \
	JITSymbol.cpp \
	Legacy.cpp \
	MCJIT.cpp \
	NullResolver.cpp \
	OrcABISupport.cpp \
	OrcCBindings.cpp \
	OrcError.cpp \
	OrcMCJITReplacement.cpp \
	RPCUtils.cpp \
	RTDyldMemoryManager.cpp \
	RuntimeDyld.cpp \
	RuntimeDyldCOFF.cpp \
	RuntimeDyldChecker.cpp \
	RuntimeDyldELF.cpp \
	RuntimeDyldELFMips.cpp \
	RuntimeDyldMachO.cpp \
	SectionMemoryManager.cpp \
	TargetSelect.cpp

ExecutionEngineCore.cpp:
	ln -s ${LLVM_SRCS}/lib/ExecutionEngine/Orc/Core.cpp $@

.PATH:	${.CURDIR}/../../../llvm/lib/ExecutionEngine
.PATH:	${.CURDIR}/../../../llvm/lib/ExecutionEngine/Interpreter
.PATH:	${.CURDIR}/../../../llvm/lib/ExecutionEngine/MCJIT
.PATH:	${.CURDIR}/../../../llvm/lib/ExecutionEngine/Orc
.PATH:	${.CURDIR}/../../../llvm/lib/ExecutionEngine/RuntimeDyld
.PATH:	${.CURDIR}/../../../llvm/lib/ExecutionEngine/RuntimeDyld/Targets

install:
	@# Nothing here so far ...

CLEANFILES+= ExecutionEngineCore.cpp

.include <bsd.lib.mk>
