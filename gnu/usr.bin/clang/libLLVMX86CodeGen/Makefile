# $OpenBSD: Makefile,v 1.11 2019/01/27 16:59:42 patrick Exp $

LIB=	LLVMX86CodeGen
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/X86 -I${LLVM_SRCS}/lib/Target/X86

.include <bsd.own.mk>
SRCS=	ShadowCallStack.cpp\
	X86AsmPrinter.cpp \
	X86AvoidStoreForwardingBlocks.cpp \
	X86CallFrameOptimization.cpp \
	X86CallLowering.cpp \
	X86CallingConv.cpp \
	X86CmovConversion.cpp \
	X86DomainReassignment.cpp \
	X86EvexToVex.cpp \
	X86ExpandPseudo.cpp \
	X86FastISel.cpp \
	X86FixupBWInsts.cpp \
	X86FixupGadgets.cpp \
	X86FixupLEAs.cpp \
	X86FixupSetCC.cpp \
	X86FlagsCopyLowering.cpp \
	X86FloatingPoint.cpp \
	X86FrameLowering.cpp \
	X86ISelDAGToDAG.cpp \
	X86ISelLowering.cpp \
	X86IndirectBranchTracking.cpp \
	X86InstrFMA3Info.cpp \
	X86InstrFoldTables.cpp \
	X86InstrInfo.cpp \
	X86InstructionSelector.cpp \
	X86InterleavedAccess.cpp \
	X86LegalizerInfo.cpp \
	X86MCInstLower.cpp \
	X86MachineFunctionInfo.cpp \
	X86MacroFusion.cpp \
	X86PadShortFunction.cpp \
	X86RegisterBankInfo.cpp \
	X86RegisterInfo.cpp \
	X86RetpolineThunks.cpp \
	X86ReturnProtectorLowering.cpp \
	X86SelectionDAGInfo.cpp \
	X86ShuffleDecodeConstantPool.cpp \
	X86SpeculativeLoadHardening.cpp \
	X86Subtarget.cpp \
	X86TargetMachine.cpp \
	X86TargetObjectFile.cpp \
	X86TargetTransformInfo.cpp \
	X86VZeroUpper.cpp \
	X86WinAllocaExpander.cpp \
	X86WinEHState.cpp \
	X86OptimizeLEAs.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/X86

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
