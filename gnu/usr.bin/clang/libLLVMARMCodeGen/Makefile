# $OpenBSD: Makefile,v 1.5 2018/04/06 14:44:04 patrick Exp $

LIB=	LLVMARMCodeGen
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/ARM -I${LLVM_SRCS}/lib/Target/ARM

.include <bsd.own.mk>
SRCS=	A15SDOptimizer.cpp \
	ARMAsmPrinter.cpp \
	ARMBaseInstrInfo.cpp \
	ARMBaseRegisterInfo.cpp \
	ARMCallLowering.cpp \
	ARMComputeBlockSize.cpp \
	ARMConstantIslandPass.cpp \
	ARMConstantPoolValue.cpp \
	ARMExpandPseudoInsts.cpp \
	ARMFastISel.cpp \
	ARMFrameLowering.cpp \
	ARMHazardRecognizer.cpp \
	ARMISelDAGToDAG.cpp \
	ARMISelLowering.cpp \
	ARMInstrInfo.cpp \
	ARMInstructionSelector.cpp \
	ARMLegalizerInfo.cpp \
	ARMLoadStoreOptimizer.cpp \
	ARMMCInstLower.cpp \
	ARMMachineFunctionInfo.cpp \
	ARMMacroFusion.cpp \
	ARMRegisterBankInfo.cpp \
	ARMRegisterInfo.cpp \
	ARMOptimizeBarriersPass.cpp \
	ARMSelectionDAGInfo.cpp \
	ARMSubtarget.cpp \
	ARMTargetMachine.cpp \
	ARMTargetObjectFile.cpp \
	ARMTargetTransformInfo.cpp \
	MLxExpansionPass.cpp \
	Thumb1FrameLowering.cpp \
	Thumb1InstrInfo.cpp \
	ThumbRegisterInfo.cpp \
	Thumb2ITBlockPass.cpp \
	Thumb2InstrInfo.cpp \
	Thumb2SizeReduction.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/ARM

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
