# $OpenBSD: Makefile,v 1.10 2021/12/17 14:55:45 patrick Exp $

LIB=	LLVMGlobalISel
NOPROFILE=

CPPFLAGS+=-I${LLVM_SRC}/include/llvm/CodeGen/GlobalISel

SRCS+=	CSEInfo.cpp \
	GISelKnownBits.cpp \
	CSEMIRBuilder.cpp \
	CallLowering.cpp \
	GlobalISel.cpp \
	Combiner.cpp \
	CombinerHelper.cpp \
	GISelChangeObserver.cpp \
	IRTranslator.cpp \
	InlineAsmLowering.cpp \
	InstructionSelect.cpp \
	InstructionSelector.cpp \
	LegalityPredicates.cpp \
	LegalizeMutations.cpp \
	Legalizer.cpp \
	LegalizerHelper.cpp \
	LegalizerInfo.cpp \
	LegacyLegalizerInfo.cpp \
	Localizer.cpp \
	LostDebugLocObserver.cpp \
	MachineIRBuilder.cpp \
	RegBankSelect.cpp \
	RegisterBank.cpp \
	RegisterBankInfo.cpp \
	GlobalISelUtils.cpp

GlobalISelUtils.cpp:
	ln -s ${LLVM_SRCS}/lib/CodeGen/GlobalISel/Utils.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/CodeGen/GlobalISel

CLEANFILES+= GlobalISelUtils.cpp
