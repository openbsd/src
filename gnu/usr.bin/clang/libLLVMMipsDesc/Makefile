# $OpenBSD: Makefile,v 1.3 2019/03/05 09:44:41 jsg Exp $

LIB=	LLVMMipsDesc
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/Mips -I${LLVM_SRCS}/lib/Target/Mips

SRCS+=	MipsABIInfo.cpp \
	MipsABIFlagsSection.cpp \
	MipsAsmBackend.cpp \
	MipsELFObjectWriter.cpp \
	MipsELFStreamer.cpp \
	MipsMCAsmInfo.cpp \
	MipsMCCodeEmitter.cpp \
	MipsMCExpr.cpp \
	MipsMCTargetDesc.cpp \
	MipsNaClELFStreamer.cpp \
	MipsOptionRecord.cpp \
	MipsTargetStreamer.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/Mips/MCTargetDesc
