# $OpenBSD: Makefile,v 1.2 2025/06/11 17:46:32 robert Exp $

LIB=	LLVMTargetParser
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/TargetParser

SRCS+=	AArch64TargetParser.cpp \
	ARMTargetParserCommon.cpp \
	ARMTargetParser.cpp \
	CSKYTargetParser.cpp \
	Host.cpp \
	LoongArchTargetParser.cpp \
	RISCVISAInfo.cpp \
	RISCVTargetParser.cpp \
	SubtargetFeature.cpp \
	TargetParser.cpp \
	Triple.cpp \
	X86TargetParser.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/TargetParser
