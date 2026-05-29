# $OpenBSD: Makefile,v 1.3 2026/05/29 11:18:31 robert Exp $

LIB=	LLVMTargetParser
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/TargetParser

SRCS+=	AArch64TargetParser.cpp \
	ARMTargetParserCommon.cpp \
	ARMTargetParser.cpp \
	CSKYTargetParser.cpp \
	Host.cpp \
	LoongArchTargetParser.cpp \
	PPCTargetParser.cpp \
	RISCVISAInfo.cpp \
	RISCVTargetParser.cpp \
	SubtargetFeature.cpp \
	TargetParser.cpp \
	Triple.cpp \
	TargetDataLayout.cpp \
	X86TargetParser.cpp \
	XtensaTargetParser.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/TargetParser
