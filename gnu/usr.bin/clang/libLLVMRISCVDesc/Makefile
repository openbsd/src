# $OpenBSD: Makefile,v 1.3 2023/11/11 18:35:37 robert Exp $

LIB=	LLVMRISCVDesc
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/RISCV \
		-I${LLVM_SRCS}/lib/Target/RISCV

SRCS+=	RISCVAsmBackend.cpp \
	RISCVBaseInfo.cpp \
	RISCVELFObjectWriter.cpp \
	RISCVInstPrinter.cpp \
	RISCVMCAsmInfo.cpp \
	RISCVMCCodeEmitter.cpp \
	RISCVMCExpr.cpp \
	RISCVMCObjectFileInfo.cpp \
	RISCVMCTargetDesc.cpp \
	RISCVMatInt.cpp \
	RISCVTargetStreamer.cpp \
	RISCVELFStreamer.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/RISCV/MCTargetDesc
