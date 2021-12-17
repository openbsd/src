# $OpenBSD: Makefile,v 1.2 2021/12/17 14:55:46 patrick Exp $

LIB=	LLVMRISCVDesc
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/RISCV \
		-I${LLVM_SRCS}/lib/Target/RISCV

SRCS+=	RISCVAsmBackend.cpp \
	RISCVBaseInfo.cpp \
	RISCVELFObjectWriter.cpp \
	RISCVELFStreamer.cpp \
	RISCVInstPrinter.cpp \
	RISCVMCAsmInfo.cpp \
	RISCVMCCodeEmitter.cpp \
	RISCVMCExpr.cpp \
	RISCVMCTargetDesc.cpp \
	RISCVMatInt.cpp \
	RISCVTargetStreamer.cpp


.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/RISCV/MCTargetDesc
