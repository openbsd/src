# $OpenBSD: Makefile,v 1.5 2021/12/17 14:55:45 patrick Exp $

LIB=	LLVMAMDGPUUtils
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AMDGPU \
		-I${LLVM_SRCS}/lib/Target/AMDGPU

SRCS+=	AMDGPUAsmUtils.cpp \
	AMDGPUBaseInfo.cpp \
	AMDGPULDSUtils.cpp \
	AMDGPUPALMetadata.cpp \
	AMDKernelCodeTUtils.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/AMDGPU/Utils
