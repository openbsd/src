# $OpenBSD: Makefile,v 1.5 2019/02/26 23:39:51 jsg Exp $

LIB=	LLVMGlobalISel
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	CallLowering.cpp \
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

install:
	@# Nothing here so far ...

CLEANFILES+= GlobalISelUtils.cpp

.include <bsd.lib.mk>
