# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:24 patrick Exp $

LIB=	LLVMAMDGPUUtils
NOPROFILE=

CPPFLAGS+=	-I${.OBJDIR}/../include/llvm/AMDGPU \
		-I${LLVM_SRCS}/lib/Target/AMDGPU

SRCS+=	AMDGPUBaseInfo.cpp \
	AMDKernelCodeTUtils.cpp \
	AMDGPUAsmUtils.cpp \
	AMDGPUPALMetadata.cpp

.PATH:	${.CURDIR}/../../../llvm/llvm/lib/Target/AMDGPU/Utils
