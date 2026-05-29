# $OpenBSD: Makefile,v 1.1 2026/05/29 11:18:31 robert Exp $

LIB=	LLVMSandboxIR
NOPROFILE=

SRCS+=	SandboxIRArgument.cpp \
	SandboxIRBasicBlock.cpp \
	SandboxIRConstant.cpp \
	SandboxIRContext.cpp \
	SandboxIRFunction.cpp \
	SandboxIRInstruction.cpp \
	SandboxIRModule.cpp \
	SandboxIRPass.cpp \
	SandboxIRPassManager.cpp \
	SandboxIRRegion.cpp \
	SandboxIRTracker.cpp \
	SandboxIRType.cpp \
	SandboxIRUser.cpp \
	SandboxIRUse.cpp \
	SandboxIRValue.cpp

SandboxIRArgument.cpp:
	ln -s ${LLVM_SRCS}/lib/SandboxIR/Argument.cpp $@

SandboxIRBasicBlock.cpp:
	ln -s ${LLVM_SRCS}/lib/SandboxIR/BasicBlock.cpp $@

SandboxIRConstant.cpp:
	ln -s ${LLVM_SRCS}/lib/SandboxIR/Constant.cpp $@

SandboxIRContext.cpp:
	ln -s ${LLVM_SRCS}/lib/SandboxIR/Context.cpp $@

SandboxIRFunction.cpp:
	ln -s ${LLVM_SRCS}/lib/SandboxIR/Function.cpp $@

SandboxIRInstruction.cpp:
	ln -s ${LLVM_SRCS}/lib/SandboxIR/Instruction.cpp $@

SandboxIRModule.cpp:
	ln -s ${LLVM_SRCS}/lib/SandboxIR/Module.cpp $@

SandboxIRPass.cpp:
	ln -s ${LLVM_SRCS}/lib/SandboxIR/Pass.cpp $@

SandboxIRPassManager.cpp:
	ln -s ${LLVM_SRCS}/lib/SandboxIR/PassManager.cpp $@

SandboxIRRegion.cpp:
	ln -s ${LLVM_SRCS}/lib/SandboxIR/Region.cpp $@

SandboxIRTracker.cpp:
	ln -s ${LLVM_SRCS}/lib/SandboxIR/Tracker.cpp $@

SandboxIRType.cpp:
	ln -s ${LLVM_SRCS}/lib/SandboxIR/Type.cpp $@

SandboxIRUser.cpp:
	ln -s ${LLVM_SRCS}/lib/SandboxIR/User.cpp $@

SandboxIRUse.cpp:
	ln -s ${LLVM_SRCS}/lib/SandboxIR/Use.cpp $@

SandboxIRValue.cpp:
	ln -s ${LLVM_SRCS}/lib/SandboxIR/Value.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/SandboxIR

CLEANFILES+=	SandboxIRArgument.cpp \
		SandboxIRBasicBlock.cpp \
		SandboxIRConstant.cpp \
		SandboxIRContext.cpp \
		SandboxIRFunction.cpp \
		SandboxIRInstruction.cpp \
		SandboxIRModule.cpp \
		SandboxIRPass.cpp \
		SandboxIRPassManager.cpp \
		SandboxIRRegion.cpp \
		SandboxIRTracker.cpp \
		SandboxIRType.cpp \
		SandboxIRUser.cpp \
		SandboxIRUse.cpp \
		SandboxIRValue.cpp
