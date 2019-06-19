# $OpenBSD: Makefile,v 1.6 2019/03/05 09:44:40 jsg Exp $

LIB=	LLVMGlobalISel
NOPROFILE=

SRCS+=	CallLowering.cpp \
	GlobalISel.cpp \
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
