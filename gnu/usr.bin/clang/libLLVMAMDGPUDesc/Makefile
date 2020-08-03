# $OpenBSD: Makefile,v 1.3 2020/08/03 14:45:24 patrick Exp $

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
	AMDGPUMCTargetDesc.cpp \
	AMDGPUTargetStreamer.cpp \
	R600MCCodeEmitter.cpp \
	R600MCTargetDesc.cpp \
	SIMCCodeEmitter.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/AMDGPU/MCTargetDesc
