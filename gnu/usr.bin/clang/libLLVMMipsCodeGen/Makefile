# $OpenBSD: Makefile,v 1.7 2019/10/25 00:40:56 mortimer Exp $

LIB=	LLVMMipsCodeGen
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Mips -I${LLVM_SRCS}/lib/Target/Mips

SRCS+=	MicroMipsSizeReduction.cpp \
	Mips16FrameLowering.cpp \
	Mips16HardFloat.cpp \
	Mips16HardFloatInfo.cpp \
	Mips16InstrInfo.cpp \
	Mips16ISelDAGToDAG.cpp \
	Mips16ISelLowering.cpp \
	Mips16RegisterInfo.cpp \
	MipsAnalyzeImmediate.cpp \
	MipsAsmPrinter.cpp \
	MipsBranchExpansion.cpp \
	MipsCCState.cpp \
	MipsCallLowering.cpp \
	MipsConstantIslandPass.cpp \
	MipsDelaySlotFiller.cpp \
	MipsExpandPseudo.cpp \
	MipsFastISel.cpp \
	MipsFrameLowering.cpp \
	MipsISelDAGToDAG.cpp \
	MipsISelLowering.cpp \
	MipsInstrInfo.cpp \
	MipsInstructionSelector.cpp \
	MipsLegalizerInfo.cpp \
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
	MipsTargetObjectFile.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/Mips
