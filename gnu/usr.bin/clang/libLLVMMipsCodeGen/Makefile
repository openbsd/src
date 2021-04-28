# $OpenBSD: Makefile,v 1.10 2021/04/28 12:55:39 patrick Exp $

LIB=	LLVMMipsCodeGen
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Mips -I${LLVM_SRCS}/lib/Target/Mips

SRCS+=	Mips16FrameLowering.cpp \
	Mips16HardFloat.cpp \
	Mips16HardFloatInfo.cpp \
	Mips16InstrInfo.cpp \
	Mips16ISelDAGToDAG.cpp \
	Mips16ISelLowering.cpp \
	Mips16RegisterInfo.cpp \
	MipsAnalyzeImmediate.cpp \
	MipsAsmPrinter.cpp \
	MipsCallLowering.cpp \
	MipsCCState.cpp \
	MipsConstantIslandPass.cpp \
	MipsDelaySlotFiller.cpp \
	MipsExpandPseudo.cpp \
	MipsFastISel.cpp \
	MipsInstrInfo.cpp \
	MipsInstructionSelector.cpp \
	MipsISelDAGToDAG.cpp \
	MipsISelLowering.cpp \
	MipsFrameLowering.cpp \
	MipsLegalizerInfo.cpp \
	MipsLoongson2FBTBFix.cpp \
	MipsBranchExpansion.cpp \
	MipsMCInstLower.cpp \
	MipsMachineFunction.cpp \
	MipsModuleISelDAGToDAG.cpp \
	MipsOptimizePICCall.cpp \
	MipsOs16.cpp \
	MipsPreLegalizerCombiner.cpp \
	MipsRegisterBankInfo.cpp \
	MipsRegisterInfo.cpp \
	MipsReturnProtectorLowering.cpp \
	MipsSEFrameLowering.cpp \
	MipsSEInstrInfo.cpp \
	MipsSEISelDAGToDAG.cpp \
	MipsSEISelLowering.cpp \
	MipsSERegisterInfo.cpp \
	MipsSubtarget.cpp \
	MipsTargetMachine.cpp \
	MipsTargetObjectFile.cpp \
	MicroMipsSizeReduction.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/Mips
