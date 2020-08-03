# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:26 patrick Exp $

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
	MipsMCExpr.cpp \
	MipsMCTargetDesc.cpp \
	MipsNaClELFStreamer.cpp \
	MipsOptionRecord.cpp \
	MipsTargetStreamer.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/Mips/MCTargetDesc
