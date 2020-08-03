# $OpenBSD: Makefile,v 1.8 2020/08/03 14:45:26 patrick Exp $

LIB=	LLVMGlobalISel
NOPROFILE=

SRCS+=	CSEInfo.cpp \
	CSEMIRBuilder.cpp \
	CallLowering.cpp \
	GlobalISel.cpp \
	GISelChangeObserver.cpp \
	GISelKnownBits.cpp \
	Combiner.cpp \
	CombinerHelper.cpp \
	IRTranslator.cpp \
	InstructionSelect.cpp \
	InstructionSelector.cpp \
	LegalityPredicates.cpp \
	LegalizeMutations.cpp \
	Legalizer.cpp \
	LegalizerHelper.cpp \
	LegalizerInfo.cpp \
	Localizer.cpp \
	MachineIRBuilder.cpp \
	RegBankSelect.cpp \
	RegisterBank.cpp \
	RegisterBankInfo.cpp \
	GlobalISelUtils.cpp

GlobalISelUtils.cpp:
	ln -s ${LLVM_SRCS}/lib/CodeGen/GlobalISel/Utils.cpp $@

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/CodeGen/GlobalISel

CLEANFILES+= GlobalISelUtils.cpp
