# $OpenBSD: Makefile,v 1.1 2023/11/11 18:35:38 robert Exp $

LIB=	LLVMTargetParser
NOPROFILE=

CPPFLAGS+=	-I${LLVM_SRCS}/include/llvm/TargetParser

SRCS+=	AArch64TargetParser.cpp \
	ARMTargetParserCommon.cpp \
	ARMTargetParser.cpp \
	CSKYTargetParser.cpp \
	Host.cpp \
	LoongArchTargetParser.cpp \
	RISCVTargetParser.cpp \
	TargetParser.cpp \
	Triple.cpp \
	X86TargetParser.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/TargetParser
