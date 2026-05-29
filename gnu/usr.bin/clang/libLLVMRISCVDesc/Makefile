# $OpenBSD: Makefile,v 1.4 2026/05/29 11:18:31 robert Exp $

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
	RISCVMachObjectWriter.cpp \
	RISCVMatInt.cpp \
	RISCVTargetStreamer.cpp \
	RISCVELFStreamer.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/RISCV/MCTargetDesc
