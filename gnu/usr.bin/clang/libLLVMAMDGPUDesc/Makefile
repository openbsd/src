# $OpenBSD: Makefile,v 1.5 2025/06/11 17:46:32 robert Exp $

LIB=	LLVMAMDGPUDesc
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AMDGPU \
		-I${LLVM_SRCS}/lib/Target/AMDGPU

SRCS+=	AMDGPUAsmBackend.cpp \
	AMDGPUELFObjectWriter.cpp \
	AMDGPUELFStreamer.cpp \
	AMDGPUInstPrinter.cpp \
	AMDGPUMCAsmInfo.cpp \
	AMDGPUMCCodeEmitter.cpp \
	AMDGPUMCExpr.cpp \
	AMDGPUMCTargetDesc.cpp \
	AMDGPUTargetStreamer.cpp \
	AMDGPUMCKernelDescriptor.cpp \
	R600InstPrinter.cpp \
	R600MCCodeEmitter.cpp \
	R600MCTargetDesc.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/AMDGPU/MCTargetDesc
