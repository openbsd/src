# $OpenBSD: Makefile,v 1.3 2018/04/06 14:44:04 patrick Exp $

LIB=	LLVMGlobalISel
NOPIC=
NOPROFILE=

.include <bsd.own.mk>
SRCS=	CallLowering.cpp \
	GlobalISel.cpp \
	IRTranslator.cpp \
	InstructionSelect.cpp \
	InstructionSelector.cpp \
	Legalizer.cpp \
	LegalizerHelper.cpp \
	LegalizerInfo.cpp \
	Localizer.cpp \
	MachineIRBuilder.cpp \
	RegBankSelect.cpp \
	RegisterBank.cpp \
	RegisterBankInfo.cpp \
	Utils.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/CodeGen/GlobalISel

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
