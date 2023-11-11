# $OpenBSD: Makefile,v 1.6 2023/11/11 18:35:36 robert Exp $

LIB=	LLVMAMDGPUUtils
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AMDGPU \
		-I${LLVM_SRCS}/lib/Target/AMDGPU

SRCS+=	AMDGPUAsmUtils.cpp \
	AMDGPUBaseInfo.cpp \
	AMDGPUMemoryUtils.cpp \
	AMDGPUPALMetadata.cpp \
	AMDKernelCodeTUtils.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/AMDGPU/Utils
