# $OpenBSD: Makefile,v 1.1 2019/01/29 00:18:23 jsg Exp $

LIB=	LLVMAMDGPUDesc
NOPIC=
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AMDGPU \
		-I${LLVM_SRCS}/lib/Target/AMDGPU

.include <bsd.own.mk>
SRCS=	AMDGPUAsmBackend.cpp \
	AMDGPUELFObjectWriter.cpp \
	AMDGPUELFStreamer.cpp \
	AMDGPUMCAsmInfo.cpp \
	AMDGPUMCCodeEmitter.cpp \
	AMDGPUMCTargetDesc.cpp \
	AMDGPUTargetStreamer.cpp \
	R600MCCodeEmitter.cpp \
	R600MCTargetDesc.cpp \
	SIMCCodeEmitter.cpp

.PATH:	${.CURDIR}/../../../llvm/lib/Target/AMDGPU/MCTargetDesc

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
