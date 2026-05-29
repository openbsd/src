# $OpenBSD: Makefile,v 1.5 2026/05/29 11:18:30 robert Exp $

LIB=	LLVMMipsDesc
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Mips -I${LLVM_SRCS}/lib/Target/Mips

SRCS+=	MipsABIInfo.cpp \
	MipsABIFlagsSection.cpp \
	MipsAsmBackend.cpp \
	MipsELFObjectWriter.cpp \
	MipsELFStreamer.cpp \
	MipsInstPrinter.cpp \
	MipsMCAsmInfo.cpp \
	MipsMCCodeEmitter.cpp \
	MipsMCTargetDesc.cpp \
	MipsOptionRecord.cpp \
	MipsTargetStreamer.cpp \
	MipsWinCOFFObjectWriter.cpp \
	MipsWinCOFFStreamer.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/Mips/MCTargetDesc
