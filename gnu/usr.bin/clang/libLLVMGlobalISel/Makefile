# $OpenBSD: Makefile,v 1.7 2019/06/23 22:07:40 patrick Exp $

LIB=	LLVMGlobalISel
NOPROFILE=

SRCS+=	CSEInfo.cpp \
	CSEMIRBuilder.cpp \
	CallLowering.cpp \
	GlobalISel.cpp \
	GISelChangeObserver.cpp \
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

.PATH:	${.CURDIR}/../../../llvm/lib/CodeGen/GlobalISel

CLEANFILES+= GlobalISelUtils.cpp
