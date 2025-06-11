# $OpenBSD: Makefile,v 1.7 2025/06/11 17:46:32 robert Exp $

LIB=	LLVMAMDGPUUtils
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AMDGPU \
		-I${LLVM_SRCS}/lib/Target/AMDGPU

SRCS+=	AMDGPUAsmUtils.cpp \
	AMDGPUBaseInfo.cpp \
	AMDGPUDelayedMCExpr.cpp \
	AMDGPUMemoryUtils.cpp \
	AMDGPUPALMetadata.cpp \
	AMDKernelCodeTUtils.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/AMDGPU/Utils
